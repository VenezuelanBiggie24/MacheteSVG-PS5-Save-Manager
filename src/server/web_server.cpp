// MacheteSVG - Embedded HTTP Web Server for PS5 Save Management
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "server/web_server.hpp"
#include "server/web_ui.hpp"
#include "server/zip_writer.hpp"
#include "core/logger.hpp"

#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

namespace machete::server
{

namespace
{

std::string url_decode(const std::string &in)
{
    std::string out;
    out.reserve(in.size());
    for (std::size_t i = 0; i < in.size(); ++i)
    {
        if (in[i] == '%' && i + 2 < in.size())
        {
            char hex_str[3] = {in[i + 1], in[i + 2], '\0'};
            char *end = nullptr;
            long val = std::strtol(hex_str, &end, 16);
            if (end != hex_str)
            {
                out += static_cast<char>(val);
                i += 2;
                continue;
            }
        }
        else if (in[i] == '+')
        {
            out += ' ';
            continue;
        }
        out += in[i];
    }
    return out;
}

std::string format_bytes(std::uint64_t bytes)
{
    char buf[64];
    if (bytes >= 1024ULL * 1024ULL * 1024ULL)
    {
        std::snprintf(buf, sizeof(buf), "%.2f GB", static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
    }
    else if (bytes >= 1024ULL * 1024ULL)
    {
        std::snprintf(buf, sizeof(buf), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    }
    else if (bytes >= 1024ULL)
    {
        std::snprintf(buf, sizeof(buf), "%.1f KB", static_cast<double>(bytes) / 1024.0);
    }
    else
    {
        std::snprintf(buf, sizeof(buf), "%llu B", static_cast<unsigned long long>(bytes));
    }
    return std::string(buf);
}

std::string json_escape(const std::string &s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s)
    {
        switch (c)
        {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20)
            {
                char hex[8];
                std::snprintf(hex, sizeof(hex), "\\u%04x", c);
                out += hex;
            }
            else
            {
                out += c;
            }
            break;
        }
    }
    return out;
}

std::string sanitize_filename(const std::string &name)
{
    std::string out;
    out.reserve(name.size());
    for (char c : name)
    {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '_' || c == '-' || c == '.')
        {
            out += c;
        }
        else if (c == ' ')
        {
            out += '_';
        }
    }
    return out.empty() ? "save" : out;
}

} // namespace

WebServer &WebServer::instance()
{
    static WebServer s_instance;
    return s_instance;
}

WebServer::WebServer() = default;

WebServer::~WebServer()
{
    stop();
}

std::string WebServer::local_ip() const
{
    if (!cached_ip_.empty())
    {
        return cached_ip_;
    }

    // Método 1: Apertura de socket UDP saliente hacia DNS público (no transmite paquetes)
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock >= 0)
    {
        struct sockaddr_in target;
        std::memset(&target, 0, sizeof(target));
        target.sin_family = AF_INET;
        target.sin_addr.s_addr = inet_addr("8.8.8.8");
        target.sin_port = htons(53);

        if (connect(sock, reinterpret_cast<struct sockaddr *>(&target), sizeof(target)) == 0)
        {
            struct sockaddr_in local;
            socklen_t len = sizeof(local);
            if (getsockname(sock, reinterpret_cast<struct sockaddr *>(&local), &len) == 0)
            {
                char buf[INET_ADDRSTRLEN] = {0};
                inet_ntop(AF_INET, &local.sin_addr, buf, sizeof(buf));
                close(sock);
                if (std::strlen(buf) > 0 && std::strcmp(buf, "0.0.0.0") != 0 && std::strcmp(buf, "127.0.0.1") != 0)
                {
                    cached_ip_ = buf;
                    return cached_ip_;
                }
            }
        }
        close(sock);
    }

    // Método 2: Recorrido de interfaces con getifaddrs
    struct ifaddrs *ifaddr = nullptr;
    if (getifaddrs(&ifaddr) == 0)
    {
        for (struct ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
        {
            if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET)
            {
                auto *p = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr);
                char buf[INET_ADDRSTRLEN] = {0};
                inet_ntop(AF_INET, &p->sin_addr, buf, sizeof(buf));
                if (std::strcmp(buf, "127.0.0.1") != 0 && std::strcmp(buf, "0.0.0.0") != 0)
                {
                    cached_ip_ = buf;
                    freeifaddrs(ifaddr);
                    return cached_ip_;
                }
            }
        }
        freeifaddrs(ifaddr);
    }

    cached_ip_ = "127.0.0.1";
    return cached_ip_;
}

std::string WebServer::url() const
{
    return "http://" + local_ip() + ":" + std::to_string(port_);
}

void WebServer::update_games(const std::vector<scanner::ScannedGame> &games)
{
    std::lock_guard<std::mutex> lock(games_mutex_);
    games_ = games;
}

std::vector<scanner::ScannedGame> WebServer::get_games() const
{
    std::lock_guard<std::mutex> lock(games_mutex_);
    return games_;
}

bool WebServer::start(std::uint16_t port)
{
    if (running_.load())
    {
        return true;
    }

    port_ = port;
    cached_ip_.clear();

    server_sock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock_ < 0)
    {
        MACHETE_LOG("[SERVER]", "Failed to create TCP socket: errno=%d", errno);
        return false;
    }

    int opt = 1;
    setsockopt(server_sock_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);

    if (bind(server_sock_, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) != 0)
    {
        MACHETE_LOG("[SERVER]", "Failed to bind to port %u: errno=%d", port_, errno);
        close(server_sock_);
        server_sock_ = -1;
        return false;
    }

    if (listen(server_sock_, 16) != 0)
    {
        MACHETE_LOG("[SERVER]", "Failed to listen on socket: errno=%d", errno);
        close(server_sock_);
        server_sock_ = -1;
        return false;
    }

    stop_requested_.store(false);
    running_.store(true);

    MACHETE_LOG("[SERVER]", "Web Server listening at %s", url().c_str());

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 4 * 1024 * 1024);

    int rc = pthread_create(
        &thread_, &attr,
        [](void *arg) -> void * {
            static_cast<WebServer *>(arg)->server_thread_main();
            return nullptr;
        },
        this);

    pthread_attr_destroy(&attr);

    if (rc != 0)
    {
        MACHETE_LOG("[SERVER]", "Failed to launch server thread: rc=%d", rc);
        running_.store(false);
        close(server_sock_);
        server_sock_ = -1;
        return false;
    }

    return true;
}

void WebServer::stop()
{
    if (!running_.load())
    {
        return;
    }

    MACHETE_LOG("[SERVER]", "Stopping Web Server...");
    stop_requested_.store(true);

    if (server_sock_ >= 0)
    {
        // Forzar cierre de socket para desbloquear cualquier accept pendiente
        shutdown(server_sock_, SHUT_RDWR);
        close(server_sock_);
        server_sock_ = -1;
    }

    if (thread_ != 0)
    {
        pthread_join(thread_, nullptr);
        thread_ = 0;
    }

    running_.store(false);
    MACHETE_LOG("[SERVER]", "Web Server stopped successfully.");
}

void WebServer::server_thread_main()
{
    while (!stop_requested_.load())
    {
        struct pollfd pfd;
        pfd.fd = server_sock_;
        pfd.events = POLLIN;
        pfd.revents = 0;

        int ret = poll(&pfd, 1, 250);
        if (ret < 0)
        {
            if (errno == EINTR) continue;
            break;
        }
        if (ret == 0)
        {
            continue; // Timeout, comprobar stop_requested
        }

        if (pfd.revents & POLLIN)
        {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(server_sock_, reinterpret_cast<struct sockaddr *>(&client_addr), &client_len);
            if (client_fd >= 0)
            {
                // Manejar petición del cliente
                handle_client(client_fd);
                close(client_fd);
            }
        }
    }
}

void WebServer::send_response(int client_fd, int status_code, const std::string &status_text,
                              const std::string &content_type, const void *body_data, std::size_t body_len,
                              const std::string &extra_headers)
{
    char header_buf[1024];
    int header_len = std::snprintf(
        header_buf, sizeof(header_buf),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "%s\r\n",
        status_code, status_text.c_str(), content_type.c_str(), body_len, extra_headers.c_str());

    send(client_fd, header_buf, static_cast<std::size_t>(header_len), 0);
    if (body_data && body_len > 0)
    {
        const auto *ptr = static_cast<const char *>(body_data);
        std::size_t remaining = body_len;
        while (remaining > 0)
        {
            ssize_t sent = send(client_fd, ptr, remaining, 0);
            if (sent <= 0) break;
            ptr += sent;
            remaining -= static_cast<std::size_t>(sent);
        }
    }
}

void WebServer::send_string_response(int client_fd, int status_code, const std::string &status_text,
                                     const std::string &content_type, const std::string &body_str,
                                     const std::string &extra_headers)
{
    send_response(client_fd, status_code, status_text, content_type, body_str.data(), body_str.size(), extra_headers);
}

void WebServer::send_file_response(int client_fd, const std::string &file_path, const std::string &filename)
{
    int fd = open(file_path.c_str(), O_RDONLY);
    if (fd < 0)
    {
        send_string_response(client_fd, 404, "Not Found", "text/plain", "File not found");
        return;
    }

    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode))
    {
        close(fd);
        send_string_response(client_fd, 403, "Forbidden", "text/plain", "Access denied");
        return;
    }

    const std::uint64_t file_size = static_cast<std::uint64_t>(st.st_size);
    std::string disposition = "Content-Disposition: attachment; filename=\"" + filename + "\"\r\n";

    char header_buf[1024];
    int header_len = std::snprintf(
        header_buf, sizeof(header_buf),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Length: %llu\r\n"
        "Connection: close\r\n"
        "%s\r\n",
        static_cast<unsigned long long>(file_size), disposition.c_str());

    send(client_fd, header_buf, static_cast<std::size_t>(header_len), 0);

    std::vector<std::uint8_t> buffer(64 * 1024);
    std::uint64_t bytes_left = file_size;
    while (bytes_left > 0)
    {
        std::size_t to_read = std::min<std::uint64_t>(bytes_left, buffer.size());
        ssize_t read_bytes = read(fd, buffer.data(), to_read);
        if (read_bytes <= 0) break;

        const char *ptr = reinterpret_cast<const char *>(buffer.data());
        std::size_t remaining = static_cast<std::size_t>(read_bytes);
        while (remaining > 0)
        {
            ssize_t sent = send(client_fd, ptr, remaining, 0);
            if (sent <= 0) break;
            ptr += sent;
            remaining -= static_cast<std::size_t>(sent);
        }
        bytes_left -= static_cast<std::uint64_t>(read_bytes);
    }
    close(fd);
}

void WebServer::handle_client(int client_fd)
{
    char req_buf[4096];
    ssize_t n = recv(client_fd, req_buf, sizeof(req_buf) - 1, 0);
    if (n <= 0)
    {
        return;
    }
    req_buf[n] = '\0';

    // Parsear primera línea: "GET /ruta HTTP/1.1"
    char method[16] = {0};
    char path_buf[1024] = {0};
    if (std::sscanf(req_buf, "%15s %1023s", method, path_buf) < 2)
    {
        send_string_response(client_fd, 400, "Bad Request", "text/plain", "Bad Request");
        return;
    }

    std::string method_str(method);
    std::string raw_path(path_buf);
    std::string decoded_path = url_decode(raw_path);

    // Separar query string si existe
    std::string path = decoded_path;
    std::size_t query_pos = path.find('?');
    if (query_pos != std::string::npos)
    {
        path = path.substr(0, query_pos);
    }

    // Ruta 1: Interfaz Web Principal
    if (path == "/" || path == "/index.html")
    {
        send_string_response(client_fd, 200, "OK", "text/html; charset=utf-8", std::string(kWebUiHtml));
        return;
    }

    // Ruta 2: API de Estado
    if (path == "/api/status")
    {
        std::string json = "{\"status\":\"running\",\"version\":\"1.0.0\",\"ip\":\"" + local_ip() +
                           "\",\"port\":" + std::to_string(port_) +
                           ",\"games_count\":" + std::to_string(get_games().size()) + "}";
        send_string_response(client_fd, 200, "OK", "application/json", json);
        return;
    }

    // Ruta 3: API de Lista de Juegos y Partidas
    if (path == "/api/games")
    {
        auto games = get_games();
        if (games.empty())
        {
            games = scanner::scan_installed_games_and_saves();
            update_games(games);
        }

        std::string json = "[";
        for (std::size_t i = 0; i < games.size(); ++i)
        {
            const auto &g = games[i];
            const bool has_save = (!g.save_path.empty() && access(g.save_path.c_str(), F_OK) == 0);

            if (i > 0) json += ",";
            json += "{";
            json += "\"title_id\":\"" + json_escape(g.title_id) + "\",";
            json += "\"title\":\"" + json_escape(g.title) + "\",";
            json += "\"platform\":\"" + json_escape(g.platform) + "\",";
            json += "\"save_path\":\"" + json_escape(g.save_path) + "\",";
            json += "\"has_save\":" + std::string(has_save ? "true" : "false") + ",";
            json += "\"size_bytes\":" + std::to_string(g.total_bytes) + ",";
            json += "\"size_str\":\"" + json_escape(g.size_str) + "\",";
            json += "\"timestamp_str\":\"" + json_escape(g.timestamp_str) + "\"";
            json += "}";
        }
        json += "]";
        send_string_response(client_fd, 200, "OK", "application/json", json);
        return;
    }

    // Ruta 4: API de Iconos de Juegos
    if (path.rfind("/api/icon/", 0) == 0)
    {
        std::string title_id = path.substr(10);
        // Buscar ruta del icono
        std::string icon_path;
        auto games = get_games();
        for (const auto &g : games)
        {
            if (g.title_id == title_id && !g.icon_path.empty() && access(g.icon_path.c_str(), F_OK) == 0)
            {
                icon_path = g.icon_path;
                break;
            }
        }

        if (icon_path.empty())
        {
            // Intentar rutas estándar
            std::string candidates[] = {
                "/user/appmeta/" + title_id + "/icon0.png",
                "/system_data/priv/appmeta/" + title_id + "/icon0.png",
                "/app0/sce_sys/icon0.png"
            };
            for (const auto &cand : candidates)
            {
                if (access(cand.c_str(), F_OK) == 0)
                {
                    icon_path = cand;
                    break;
                }
            }
        }

        if (!icon_path.empty())
        {
            int fd = open(icon_path.c_str(), O_RDONLY);
            if (fd >= 0)
            {
                struct stat st;
                if (fstat(fd, &st) == 0 && S_ISREG(st.st_mode))
                {
                    std::vector<std::uint8_t> icon_data(static_cast<std::size_t>(st.st_size));
                    if (read(fd, icon_data.data(), icon_data.size()) == static_cast<ssize_t>(icon_data.size()))
                    {
                        close(fd);
                        send_response(client_fd, 200, "OK", "image/png", icon_data.data(), icon_data.size(),
                                      "Cache-Control: public, max-age=86400\r\n");
                        return;
                    }
                }
                close(fd);
            }
        }

        send_string_response(client_fd, 404, "Not Found", "text/plain", "Icon not found");
        return;
    }

    // Ruta 5: API de Descarga Comprimida (.zip) de una Partida Individual
    if (path.rfind("/api/download/", 0) == 0)
    {
        std::string title_id = path.substr(14);
        auto games = get_games();
        const scanner::ScannedGame *target_game = nullptr;
        for (const auto &g : games)
        {
            if (g.title_id == title_id)
            {
                target_game = &g;
                break;
            }
        }

        if (!target_game || target_game->save_path.empty() || access(target_game->save_path.c_str(), F_OK) != 0)
        {
            send_string_response(client_fd, 404, "Not Found", "text/plain", "Save data not found for title");
            return;
        }

        ZipWriter zip;
        zip.add_directory(target_game->save_path, target_game->title_id);

        // Si es PS5, incluir también metadatos si existen en savedata_prospero_meta
        std::string meta_path = target_game->save_path;
        std::size_t pos = meta_path.find("savedata_prospero");
        if (pos != std::string::npos)
        {
            meta_path.replace(pos, 17, "savedata_prospero_meta");
            if (access(meta_path.c_str(), F_OK) == 0)
            {
                zip.add_directory(meta_path, target_game->title_id + "/sce_meta");
            }
        }

        // Si existe keystone en /user/appmeta/<title_id>/sce_sys/keystone, agregarlo
        std::string keystone_path = "/user/appmeta/" + target_game->title_id + "/sce_sys/keystone";
        if (access(keystone_path.c_str(), F_OK) == 0)
        {
            zip.add_file(keystone_path, target_game->title_id + "/keystone");
        }

        zip.prepare();
        if (zip.entry_count() == 0)
        {
            send_string_response(client_fd, 404, "Not Found", "text/plain", "Save folder contains no files");
            return;
        }

        std::string filename = target_game->title_id + "_" + sanitize_filename(target_game->title) + "_save.zip";
        std::uint64_t total_size = zip.total_zip_size();

        char header_buf[1024];
        int header_len = std::snprintf(
            header_buf, sizeof(header_buf),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/zip\r\n"
            "Content-Disposition: attachment; filename=\"%s\"\r\n"
            "Content-Length: %llu\r\n"
            "Connection: close\r\n\r\n",
            filename.c_str(), static_cast<unsigned long long>(total_size));

        send(client_fd, header_buf, static_cast<std::size_t>(header_len), 0);
        zip.stream_to_fd(client_fd);
        return;
    }

    // Ruta 6: API de Descarga Comprimida de TODAS las Partidas
    if (path == "/api/download_all")
    {
        auto games = get_games();
        ZipWriter zip;
        for (const auto &g : games)
        {
            if (!g.save_path.empty() && access(g.save_path.c_str(), F_OK) == 0)
            {
                std::string folder_name = g.title_id + " - " + sanitize_filename(g.title);
                zip.add_directory(g.save_path, folder_name);
            }
        }

        zip.prepare();
        if (zip.entry_count() == 0)
        {
            send_string_response(client_fd, 404, "Not Found", "text/plain", "No save games detected on console");
            return;
        }

        std::string filename = "MacheteSVG_All_Saves.zip";
        std::uint64_t total_size = zip.total_zip_size();

        char header_buf[1024];
        int header_len = std::snprintf(
            header_buf, sizeof(header_buf),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/zip\r\n"
            "Content-Disposition: attachment; filename=\"%s\"\r\n"
            "Content-Length: %llu\r\n"
            "Connection: close\r\n\r\n",
            filename.c_str(), static_cast<unsigned long long>(total_size));

        send(client_fd, header_buf, static_cast<std::size_t>(header_len), 0);
        zip.stream_to_fd(client_fd);
        return;
    }

    // Ruta 7: API de Exploración de Archivos Individuales de una Partida
    if (path.rfind("/api/files/", 0) == 0)
    {
        std::string title_id = path.substr(11);
        auto games = get_games();
        const scanner::ScannedGame *target_game = nullptr;
        for (const auto &g : games)
        {
            if (g.title_id == title_id)
            {
                target_game = &g;
                break;
            }
        }

        if (!target_game || target_game->save_path.empty())
        {
            send_string_response(client_fd, 404, "Not Found", "application/json", "[]");
            return;
        }

        DIR *dir = opendir(target_game->save_path.c_str());
        if (!dir)
        {
            send_string_response(client_fd, 404, "Not Found", "application/json", "[]");
            return;
        }

        std::string json = "[";
        bool first = true;
        struct dirent *de;
        while ((de = readdir(dir)) != nullptr)
        {
            if (std::strcmp(de->d_name, ".") == 0 || std::strcmp(de->d_name, "..") == 0)
            {
                continue;
            }

            std::string full_path = target_game->save_path + "/" + de->d_name;
            struct stat st;
            if (stat(full_path.c_str(), &st) == 0 && S_ISREG(st.st_mode))
            {
                if (!first) json += ",";
                first = false;
                json += "{";
                json += "\"name\":\"" + json_escape(de->d_name) + "\",";
                json += "\"size_bytes\":" + std::to_string(st.st_size) + ",";
                json += "\"size_formatted\":\"" + format_bytes(static_cast<std::uint64_t>(st.st_size)) + "\"";
                json += "}";
            }
        }
        closedir(dir);
        json += "]";

        send_string_response(client_fd, 200, "OK", "application/json", json);
        return;
    }

    // Ruta 8: Descarga de Archivo Individual de una Partida
    if (path.rfind("/api/file/", 0) == 0)
    {
        std::string rest = path.substr(10);
        std::size_t slash = rest.find('/');
        if (slash == std::string::npos)
        {
            send_string_response(client_fd, 400, "Bad Request", "text/plain", "Missing file parameter");
            return;
        }

        std::string title_id = rest.substr(0, slash);
        std::string filename = rest.substr(slash + 1);

        // Seguridad: Prohibir directory traversal
        if (filename.find("..") != std::string::npos || filename.find('/') != std::string::npos)
        {
            send_string_response(client_fd, 403, "Forbidden", "text/plain", "Access denied");
            return;
        }

        auto games = get_games();
        const scanner::ScannedGame *target_game = nullptr;
        for (const auto &g : games)
        {
            if (g.title_id == title_id)
            {
                target_game = &g;
                break;
            }
        }

        if (!target_game || target_game->save_path.empty())
        {
            send_string_response(client_fd, 404, "Not Found", "text/plain", "Save data not found");
            return;
        }

        std::string target_file = target_game->save_path + "/" + filename;
        send_file_response(client_fd, target_file, filename);
        return;
    }

    send_string_response(client_fd, 404, "Not Found", "text/plain", "Endpoint not found");
}

} // namespace machete::server
