// MacheteSVG - Embedded HTTP Web Server for PS5 Save Management
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "server/web_server.hpp"
#include "server/web_ui.hpp"
#include "server/zip_writer.hpp"
#include "server/zip_reader.hpp"
#include "core/build_info.hpp"
#include "core/logger.hpp"
#include "core/pfs_mount_manager.hpp"
#include "core/samu_vault.hpp"
#include "core/save_manager.hpp"
#include "core/storage.hpp"

#include <arpa/inet.h>
#include <csignal>
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
#include <sys/time.h>
#include <unistd.h>

#ifndef SO_NOSIGPIPE
#if defined(__FreeBSD__) || defined(__PS5__) || defined(__prospero__) || defined(__APPLE__)
#define SO_NOSIGPIPE 0x0800
#endif
#endif

namespace machete::server
{

namespace
{

class ScopedFd
{
public:
    ScopedFd() noexcept : fd_(-1) {}
    explicit ScopedFd(int fd) noexcept : fd_(fd) {}
    ~ScopedFd() noexcept { reset(); }

    ScopedFd(const ScopedFd &) = delete;
    ScopedFd &operator=(const ScopedFd &) = delete;

    ScopedFd(ScopedFd &&other) noexcept : fd_(other.fd_) { other.fd_ = -1; }
    ScopedFd &operator=(ScopedFd &&other) noexcept
    {
        if (this != &other)
        {
            reset();
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    int get() const noexcept { return fd_; }
    bool is_valid() const noexcept { return fd_ >= 0; }
    explicit operator bool() const noexcept { return fd_ >= 0; }

    int release() noexcept
    {
        int ret = fd_;
        fd_ = -1;
        return ret;
    }

    void reset(int new_fd = -1) noexcept
    {
        if (fd_ >= 0)
        {
            ::close(fd_);
        }
        fd_ = new_fd;
    }

private:
    int fd_{-1};
};

class ScopedDir
{
public:
    ScopedDir() noexcept : dir_(nullptr) {}
    explicit ScopedDir(DIR *dir) noexcept : dir_(dir) {}
    ~ScopedDir() noexcept { reset(); }

    ScopedDir(const ScopedDir &) = delete;
    ScopedDir &operator=(const ScopedDir &) = delete;

    ScopedDir(ScopedDir &&other) noexcept : dir_(other.dir_) { other.dir_ = nullptr; }
    ScopedDir &operator=(ScopedDir &&other) noexcept
    {
        if (this != &other)
        {
            reset();
            dir_ = other.dir_;
            other.dir_ = nullptr;
        }
        return *this;
    }

    DIR *get() const noexcept { return dir_; }
    bool is_valid() const noexcept { return dir_ != nullptr; }
    explicit operator bool() const noexcept { return dir_ != nullptr; }

    DIR *release() noexcept
    {
        DIR *ret = dir_;
        dir_ = nullptr;
        return ret;
    }

    void reset(DIR *new_dir = nullptr) noexcept
    {
        if (dir_)
        {
            ::closedir(dir_);
        }
        dir_ = new_dir;
    }

private:
    DIR *dir_{nullptr};
};

static bool send_all_socket(int fd, const void *data, std::size_t len)
{
    if (!data || len == 0)
    {
        return true;
    }
    const char *ptr = static_cast<const char *>(data);
    std::size_t remaining = len;
    while (remaining > 0)
    {
        ssize_t sent = send(fd, ptr, remaining, 0);
        if (sent < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return false;
        }
        if (sent == 0)
        {
            return false;
        }
        ptr += sent;
        remaining -= static_cast<std::size_t>(sent);
    }
    return true;
}

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

struct BatchJsonRequest
{
    bool valid = false;
    std::string error_message;
    std::vector<std::string> title_ids;
    std::string custom_root;
};

static BatchJsonRequest parse_batch_json_payload(const std::string &body)
{
    BatchJsonRequest req;

    const auto l = body.find_first_not_of(" \t\r\n");
    const auto r = body.find_last_not_of(" \t\r\n");
    if (l == std::string::npos || r == std::string::npos || l > r)
    {
        req.valid = false;
        req.error_message = "Cuerpo de solicitud vacio o ausente";
        return req;
    }

    const std::string_view trimmed(body.data() + l, r - l + 1);

    if (trimmed.front() != '{' || trimmed.back() != '}')
    {
        req.valid = false;
        req.error_message = "Formato JSON malformado: debe ser un objeto JSON que inicie con '{' y finalice con '}'";
        return req;
    }

    std::size_t key_pos = body.find("\"title_ids\"");
    std::string key_used = "title_ids";
    if (key_pos == std::string::npos)
    {
        key_pos = body.find("\"titles\"");
        key_used = "titles";
    }

    if (key_pos == std::string::npos)
    {
        req.valid = false;
        req.error_message = "Campo requerido ausente: debe incluir 'title_ids' o 'titles' en el objeto JSON";
        return req;
    }

    const std::size_t colon_pos = body.find(':', key_pos + key_used.size() + 2);
    if (colon_pos == std::string::npos)
    {
        req.valid = false;
        req.error_message = "Sintaxis JSON invalida: dos puntos ':' no encontrados tras la clave de titulos";
        return req;
    }

    const std::size_t arr_start = body.find_first_not_of(" \t\r\n", colon_pos + 1);
    if (arr_start == std::string::npos || body[arr_start] != '[')
    {
        req.valid = false;
        req.error_message = "Tipo invalido: el campo '" + key_used + "' debe ser un array JSON (iniciado con '[')";
        return req;
    }

    const std::size_t arr_end = body.find(']', arr_start + 1);
    if (arr_end == std::string::npos)
    {
        req.valid = false;
        req.error_message = "Formato JSON malformado: corchete de cierre ']' no encontrado para el array '" + key_used + "'";
        return req;
    }

    const std::string arr_content = body.substr(arr_start + 1, arr_end - (arr_start + 1));
    std::size_t p = 0;
    while (p < arr_content.size())
    {
        while (p < arr_content.size() && (arr_content[p] == ' ' || arr_content[p] == '\t' ||
                                          arr_content[p] == '\r' || arr_content[p] == '\n' ||
                                          arr_content[p] == ','))
        {
            p++;
        }
        if (p >= arr_content.size())
            break;

        if (arr_content[p] != '"')
        {
            req.valid = false;
            req.error_message = "Tipo invalido en array: todos los elementos de '" + key_used + "' deben ser cadenas entre comillas";
            return req;
        }

        const std::size_t q2 = arr_content.find('"', p + 1);
        if (q2 == std::string::npos)
        {
            req.valid = false;
            req.error_message = "Formato JSON malformado: cadena sin cerrar dentro del array '" + key_used + "'";
            return req;
        }

        const std::string val = arr_content.substr(p + 1, q2 - (p + 1));
        if (val.empty() || val.find('/') != std::string::npos || val.find('\\') != std::string::npos ||
            val.find("..") != std::string::npos)
        {
            req.valid = false;
            req.error_message = "Title ID invalido en la lista: " + (val.empty() ? "(vacio)" : val);
            return req;
        }

        req.title_ids.push_back(val);
        p = q2 + 1;
    }

    if (req.title_ids.empty())
    {
        req.valid = false;
        req.error_message = "La lista '" + key_used + "' no contiene ningun juego (array vacio o sin elementos validos)";
        return req;
    }

    for (const char *rk : {"\"destination_root\"", "\"backup_root\"", "\"root\""})
    {
        const std::size_t rk_pos = body.find(rk);
        if (rk_pos != std::string::npos)
        {
            const std::size_t c_pos = body.find(':', rk_pos);
            if (c_pos != std::string::npos)
            {
                const std::size_t q1 = body.find('"', c_pos);
                if (q1 != std::string::npos)
                {
                    const std::size_t q2 = body.find('"', q1 + 1);
                    if (q2 != std::string::npos)
                    {
                        const std::string r_val = body.substr(q1 + 1, q2 - (q1 + 1));
                        if (r_val.find("..") != std::string::npos)
                        {
                            req.valid = false;
                            req.error_message = "Ruta de directorio invalida (contiene secuencia de escape '..')";
                            return req;
                        }
                        req.custom_root = r_val;
                        break;
                    }
                }
            }
        }
    }

    req.valid = true;
    return req;
}

constexpr const char *kStorageRoots[] = {
    "/data/MacheteSVG",
    "/mnt/ext1/MacheteSVG",
    "/mnt/ext0/MacheteSVG",
    "/mnt/usb0/MacheteSVG",
    "/mnt/usb1/MacheteSVG",
    "/mnt/usb2/MacheteSVG",
    "/mnt/usb3/MacheteSVG",
    "/mnt/usb4/MacheteSVG",
    "/mnt/usb5/MacheteSVG",
    "/mnt/usb6/MacheteSVG",
    "/mnt/usb7/MacheteSVG"
};

std::string resolve_or_create_crypto_vault(const scanner::ScannedGame &game)
{
    if (game.save_path.empty())
    {
        return "";
    }

    // 1. Si ya existe un respaldo previo con crypto_vault.bin en cualquier unidad de almacenamiento
    for (const char *root : kStorageRoots)
    {
        ScopedDir dir(opendir(root));
        if (!dir.is_valid())
            continue;

        while (const dirent *ent = readdir(dir.get()))
        {
            if (ent->d_name[0] == '.')
                continue;
            std::string name = ent->d_name;
            if (name.rfind(game.title_id, 0) == 0)
            {
                std::string cand = std::string(root) + "/" + name + "/crypto_vault.bin";
                if (access(cand.c_str(), F_OK) == 0)
                {
                    return cand;
                }
            }
        }
    }

    // 2. Si ya fue generado previamente en la zona de staging
    std::string staging_vault = std::string(machete::pfs::SAVE_STAGING_DIR) + "/vault/" + game.title_id + "/crypto_vault.bin";
    if (access(staging_vault.c_str(), F_OK) == 0)
    {
        return staging_vault;
    }

    // 3. Generar el vault al vuelo en la zona de staging segura (/data/save_staging/vault/)
    std::string vault_base = std::string(machete::pfs::SAVE_STAGING_DIR) + "/vault";
    machete::pfs::ensure_dir_tree(vault_base);
    std::string game_vault_dir = vault_base + "/" + game.title_id;
    machete::pfs::ensure_dir_tree(game_vault_dir);

    std::uint32_t user_id = 0x10000000;
    std::size_t pos = game.save_path.find("/user/home/");
    if (pos != std::string::npos)
    {
        std::size_t start = pos + 11;
        std::size_t end = game.save_path.find('/', start);
        std::string uid_str = (end == std::string::npos) ? game.save_path.substr(start) : game.save_path.substr(start, end - start);
        unsigned long val = std::strtoul(uid_str.c_str(), nullptr, 16);
        if (val != 0)
        {
            user_id = static_cast<std::uint32_t>(val);
        }
    }

    if (machete::vault::archive_crypto_vault(game_vault_dir, game.title_id, game.platform, game.save_path, user_id))
    {
        std::string out_file = game_vault_dir + "/crypto_vault.bin";
        if (access(out_file.c_str(), F_OK) == 0)
        {
            return out_file;
        }
    }

    return "";
}

std::string resolve_or_extract_decrypted_dir(const scanner::ScannedGame &game)
{
    if (game.save_path.empty())
    {
        return "";
    }

    // 1. Si ya existe carpeta decrypted/ en un respaldo previo en cualquier unidad de almacenamiento
    for (const char *root : kStorageRoots)
    {
        ScopedDir dir(opendir(root));
        if (!dir.is_valid())
            continue;

        while (const dirent *ent = readdir(dir.get()))
        {
            if (ent->d_name[0] == '.')
                continue;
            std::string name = ent->d_name;
            if (name.rfind(game.title_id, 0) == 0)
            {
                std::string folder = std::string(root) + "/" + name;
                if (machete::pfs::has_decrypted_payload(folder))
                {
                    return folder + "/decrypted";
                }
            }
        }
    }

    // 2. Si ya fue extraido previamente en la zona de staging
    std::string staging_dir = std::string(machete::pfs::SAVE_STAGING_DIR) + "/vault/" + game.title_id;
    if (machete::pfs::has_decrypted_payload(staging_dir))
    {
        return staging_dir + "/decrypted";
    }

    // 3. Extraer al vuelo mediante /dev/pfsmgr + ScopedPfsMount en /data/save_staging/vault/
    std::string vault_base = std::string(machete::pfs::SAVE_STAGING_DIR) + "/vault";
    machete::pfs::ensure_dir_tree(vault_base);
    std::string game_vault_dir = vault_base + "/" + game.title_id;
    machete::pfs::ensure_dir_tree(game_vault_dir);

    const bool is_ps4 = (game.platform == "PS4");
    if (machete::pfs::extract_decrypted_slots_to_backup(game.save_path, game_vault_dir, game.title_id, !is_ps4) > 0)
    {
        std::string dec_dir = game_vault_dir + "/decrypted";
        if (access(dec_dir.c_str(), F_OK) == 0)
        {
            return dec_dir;
        }
    }

    return "";
}

std::string resolve_authentic_keystone(const scanner::ScannedGame &game)
{
    // 1. /user/appmeta/<title_id>/sce_sys/keystone
    std::string appmeta_ks = "/user/appmeta/" + game.title_id + "/sce_sys/keystone";
    if (access(appmeta_ks.c_str(), R_OK) == 0)
    {
        return appmeta_ks;
    }

    // 2. /data/MacheteSVG/keystones/<title_id>.keystone
    std::string global_ks = "/data/MacheteSVG/keystones/" + game.title_id + ".keystone";
    if (access(global_ks.c_str(), R_OK) == 0)
    {
        return global_ks;
    }

    // 3. Staging vault: /data/save_staging/vault/<title_id>/keystone
    std::string staging_ks = std::string(machete::pfs::SAVE_STAGING_DIR) + "/vault/" + game.title_id + "/keystone";
    if (access(staging_ks.c_str(), R_OK) == 0)
    {
        return staging_ks;
    }

    // 4. Respaldos existentes en cualquier unidad
    for (const char *root : kStorageRoots)
    {
        ScopedDir dir(opendir(root));
        if (!dir.is_valid())
            continue;

        while (const dirent *ent = readdir(dir.get()))
        {
            if (ent->d_name[0] == '.')
                continue;
            std::string name = ent->d_name;
            if (name.rfind(game.title_id, 0) == 0)
            {
                std::string cand = std::string(root) + "/" + name + "/keystone";
                if (access(cand.c_str(), R_OK) == 0)
                {
                    return cand;
                }
            }
        }
    }

    return "";
}

} // namespace

WebServer &WebServer::instance()
{
    static WebServer s_instance;
    return s_instance;
}

WebServer::WebServer()
{
    std::signal(SIGPIPE, SIG_IGN);
}

WebServer::~WebServer()
{
    stop();
}

std::string WebServer::local_ip() const
{
    std::lock_guard<std::mutex> lock(ip_mutex_);
    if (!cached_ip_.empty())
    {
        return cached_ip_;
    }

    // Metodo 1: Apertura de socket UDP saliente hacia DNS publico (no transmite paquetes)
    ScopedFd sock(socket(AF_INET, SOCK_DGRAM, 0));
    if (sock.is_valid())
    {
        struct sockaddr_in target{};
        target.sin_family = AF_INET;
        target.sin_addr.s_addr = inet_addr("8.8.8.8");
        target.sin_port = htons(53);

        if (connect(sock.get(), reinterpret_cast<struct sockaddr *>(&target), sizeof(target)) == 0)
        {
            struct sockaddr_in local{};
            socklen_t len = sizeof(local);
            if (getsockname(sock.get(), reinterpret_cast<struct sockaddr *>(&local), &len) == 0)
            {
                char buf[INET_ADDRSTRLEN] = {0};
                inet_ntop(AF_INET, &local.sin_addr, buf, sizeof(buf));
                if (std::strlen(buf) > 0 && std::strcmp(buf, "0.0.0.0") != 0 && std::strcmp(buf, "127.0.0.1") != 0)
                {
                    cached_ip_ = buf;
                    return cached_ip_;
                }
            }
        }
    }

    // Metodo 2: Recorrido de interfaces con getifaddrs
    struct ifaddrs *ifaddr = nullptr;
    if (getifaddrs(&ifaddr) == 0 && ifaddr != nullptr)
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

bool WebServer::find_game(const std::string &title_id, scanner::ScannedGame &out) const
{
    std::lock_guard<std::mutex> lock(games_mutex_);
    for (const auto &g : games_)
    {
        if (g.title_id == title_id)
        {
            out = g;
            return true;
        }
    }
    return false;
}

bool WebServer::start(std::uint16_t port)
{
    if (running_.load())
    {
        return true;
    }

    std::signal(SIGPIPE, SIG_IGN);

    port_ = port;
    {
        std::lock_guard<std::mutex> lock(ip_mutex_);
        cached_ip_.clear();
    }

    server_sock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock_ < 0)
    {
        MACHETE_LOG("[SERVER]", "Failed to create TCP socket: errno=%d", errno);
        return false;
    }

    int opt = 1;
    if (setsockopt(server_sock_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != 0)
    {
        MACHETE_LOG("[SERVER]", "setsockopt SO_REUSEADDR warning: errno=%d", errno);
    }

#ifdef SO_REUSEPORT
    if (setsockopt(server_sock_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) != 0)
    {
        MACHETE_LOG("[SERVER]", "setsockopt SO_REUSEPORT warning: errno=%d", errno);
    }
#endif

#ifdef SO_NOSIGPIPE
    if (setsockopt(server_sock_, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof(opt)) != 0)
    {
        MACHETE_LOG("[SERVER]", "setsockopt SO_NOSIGPIPE warning: errno=%d", errno);
    }
#endif

    struct sockaddr_in addr{};
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
        struct pollfd pfd{};
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
            struct sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(server_sock_, reinterpret_cast<struct sockaddr *>(&client_addr), &client_len);
            if (client_fd >= 0)
            {
#ifdef SO_NOSIGPIPE
                int opt_nosigpipe = 1;
                setsockopt(client_fd, SOL_SOCKET, SO_NOSIGPIPE, &opt_nosigpipe, sizeof(opt_nosigpipe));
#endif
                struct timeval tv{};
                tv.tv_sec = 15;
                tv.tv_usec = 0;
                setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
                setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

                handle_client(client_fd);
                close(client_fd);
            }
            else
            {
                if (errno == EINTR || errno == ECONNABORTED || errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    continue;
                }
                if (stop_requested_.load())
                {
                    break;
                }
                MACHETE_LOG("[SERVER]", "accept() failed: errno=%d", errno);
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
        "Cache-Control: no-cache, no-store, must-revalidate\r\n"
        "Pragma: no-cache\r\n"
        "Expires: 0\r\n"
        "%s\r\n",
        status_code, status_text.c_str(), content_type.c_str(), body_len, extra_headers.c_str());

    if (header_len <= 0 || static_cast<std::size_t>(header_len) >= sizeof(header_buf))
    {
        return;
    }

    if (!send_all_socket(client_fd, header_buf, static_cast<std::size_t>(header_len)))
    {
        return;
    }

    if (body_data && body_len > 0)
    {
        send_all_socket(client_fd, body_data, body_len);
    }
}

void WebServer::send_string_response(int client_fd, int status_code, const std::string &status_text,
                                     const std::string &content_type, const std::string &body_str,
                                     const std::string &extra_headers)
{
    send_response(client_fd, status_code, status_text, content_type, body_str.data(), body_str.size(), extra_headers);
}

void WebServer::send_file_response(int client_fd, const std::string &file_path, const std::string &filename,
                                   const std::string &content_type)
{
    ScopedFd fd(open(file_path.c_str(), O_RDONLY));
    if (!fd.is_valid())
    {
        send_string_response(client_fd, 404, "Not Found", "text/plain", "File not found");
        return;
    }

    struct stat st{};
    if (fstat(fd.get(), &st) != 0 || !S_ISREG(st.st_mode))
    {
        send_string_response(client_fd, 403, "Forbidden", "text/plain", "Access denied");
        return;
    }

    const std::uint64_t file_size = static_cast<std::uint64_t>(st.st_size);
    std::string disposition = "Content-Disposition: attachment; filename=\"" + filename + "\"\r\n";

    char header_buf[1024];
    int header_len = std::snprintf(
        header_buf, sizeof(header_buf),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %llu\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-cache, no-store, must-revalidate\r\n"
        "Pragma: no-cache\r\n"
        "Expires: 0\r\n"
        "%s\r\n",
        content_type.c_str(),
        static_cast<unsigned long long>(file_size), disposition.c_str());

    if (header_len <= 0 || static_cast<std::size_t>(header_len) >= sizeof(header_buf))
    {
        return;
    }

    if (!send_all_socket(client_fd, header_buf, static_cast<std::size_t>(header_len)))
    {
        return;
    }

    constexpr std::size_t kBufferSize = 64 * 1024;
    std::vector<std::uint8_t> buffer(kBufferSize);
    std::uint64_t bytes_left = file_size;
    while (bytes_left > 0)
    {
        std::size_t to_read = std::min<std::uint64_t>(bytes_left, buffer.size());
        ssize_t read_bytes = read(fd.get(), buffer.data(), to_read);
        if (read_bytes < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            break;
        }
        if (read_bytes == 0)
        {
            break;
        }

        if (!send_all_socket(client_fd, buffer.data(), static_cast<std::size_t>(read_bytes)))
        {
            break;
        }
        bytes_left -= static_cast<std::uint64_t>(read_bytes);
    }
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

    // Parsear primera linea: "GET /ruta HTTP/1.1"
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
        std::string json = "{\"status\":\"running\",\"version\":\"" + std::string(::machete::build::kAppVersion) +
                           "\",\"build\":\"" + std::string(::machete::build::kBuildNumber) +
                           "\",\"commit\":\"" + std::string(::machete::build::kGitCommit) +
                           "\",\"build_date\":\"" + std::string(::machete::build::kBuildDate) + " " + std::string(::machete::build::kBuildTime) +
                           "\",\"ip\":\"" + local_ip() +
                           "\",\"port\":" + std::to_string(port_) +
                           ",\"updater_enabled\":" + std::string(is_update_module_enabled() ? "true" : "false") +
                           ",\"games_count\":" + std::to_string(get_games().size()) + "}";
        send_string_response(client_fd, 200, "OK", "application/json", json);
        return;
    }

    // Ruta: Diagnostico de Verdad-Terreno PFS (/api/dump?title_id=...)
    if (path == "/api/dump" || path.rfind("/api/dump?", 0) == 0)
    {
        std::string query_tid;
        const auto qpos = path.find('?');
        if (qpos != std::string::npos)
        {
            const std::string query = path.substr(qpos + 1);
            const auto tpos = query.find("title_id=");
            if (tpos != std::string::npos)
            {
                query_tid = query.substr(tpos + 9);
                const auto amp = query_tid.find('&');
                if (amp != std::string::npos)
                    query_tid = query_tid.substr(0, amp);
            }
        }

        const int dumped_count = machete::pfs::dump_installed_save_containers(query_tid, "MANUAL_API_TRIGGER");
        std::string json = "{\"status\":\"ok\",\"title_id\":\"" + query_tid +
                           "\",\"dumped_containers\":" + std::to_string(dumped_count) +
                           ",\"message\":\"Diagnostic dump completed. Inspect machete.log\"}";
        send_string_response(client_fd, 200, "OK", "application/json", json);
        return;
    }

    // Ruta: Descarga del Registro de Eventos del Sistema (machete.log)
    if (path == "/api/log" || path == "/log" || path == "/machete.log")
    {
        const std::string log_candidates[] = {
            "/data/homebrew/PPSA99700/machete.log",
            "/data/machete.log",
            "/download0/machete.log",
            "/user/app/PPSA99700/machete.log",
            "machete.log"
        };
        for (const auto &cand : log_candidates)
        {
            if (access(cand.c_str(), R_OK) == 0)
            {
                send_file_response(client_fd, cand, "machete.log", "text/plain; charset=utf-8");
                return;
            }
        }
        std::string fallback = "[SYSTEM] MacheteSaveGameManager v" + std::string(::machete::build::kAppVersion) +
                               " (Build #" + std::string(::machete::build::kBuildNumber) +
                               ", Commit " + std::string(::machete::build::kGitCommit) +
                               ") - No log file found on disk yet.\n";
        send_string_response(client_fd, 200, "OK", "text/plain; charset=utf-8", fallback);
        return;
    }

    // Ruta: Logotipo Oficial de la Aplicacion (icon0.png)
    if (path == "/api/logo" || path == "/logo.png" || path == "/icon0.png")
    {
        const std::string logo_candidates[] = {
            "/app0/sce_sys/icon0.png",
            "/data/homebrew/PPSA99700/sce_sys/icon0.png",
            "sce_sys/icon0.png",
#ifdef HUI_SOURCE_DIR
            std::string(HUI_SOURCE_DIR) + "/sce_sys/icon0.png",
#endif
            "/user/app/PPSA99700/sce_sys/icon0.png"
        };
        for (const auto &cand : logo_candidates)
        {
            ScopedFd fd(open(cand.c_str(), O_RDONLY));
            if (fd.is_valid())
            {
                struct stat st{};
                if (fstat(fd.get(), &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0 && st.st_size <= 16 * 1024 * 1024)
                {
                    std::vector<std::uint8_t> logo_data(static_cast<std::size_t>(st.st_size));
                    std::size_t total_read = 0;
                    bool read_ok = true;
                    while (total_read < logo_data.size())
                    {
                        ssize_t r = read(fd.get(), logo_data.data() + total_read, logo_data.size() - total_read);
                        if (r < 0)
                        {
                            if (errno == EINTR) continue;
                            read_ok = false;
                            break;
                        }
                        if (r == 0) break;
                        total_read += static_cast<std::size_t>(r);
                    }
                    if (read_ok && total_read == logo_data.size())
                    {
                        send_response(client_fd, 200, "OK", "image/png", logo_data.data(), logo_data.size(),
                                      "Cache-Control: public, max-age=86400\r\n");
                        return;
                    }
                }
            }
        }
        send_string_response(client_fd, 404, "Not Found", "text/plain", "Logo not found");
        return;
    }

    // Ruta: Animaciones de Shin-chan para el Actualizador Web (GIFs)
    if (path.rfind("/api/shinchan", 0) == 0)
    {
        std::string anim_type = "dance";
        if (decoded_path.find("done") != std::string::npos || decoded_path.find("success") != std::string::npos)
        {
            anim_type = "done";
        }
        else if (decoded_path.find("error") != std::string::npos || decoded_path.find("crying") != std::string::npos ||
                 decoded_path.find("fail") != std::string::npos || decoded_path.find("fallo") != std::string::npos)
        {
            anim_type = "error";
        }

        const std::string filename = "shinchan_" + anim_type + ".gif";
        const std::string anim_candidates[] = {
            "assets/ui/" + filename,
            "/app0/assets/ui/" + filename,
            "/data/homebrew/PPSA99700/assets/ui/" + filename,
            "/user/app/PPSA99700/assets/ui/" + filename,
#ifdef HUI_SOURCE_DIR
            std::string(HUI_SOURCE_DIR) + "/assets/ui/" + filename,
#endif
            "/tmp/machete_test_homebrew/PPSA99700/assets/ui/" + filename
        };

        for (const auto &cand : anim_candidates)
        {
            ScopedFd fd(open(cand.c_str(), O_RDONLY));
            if (fd.is_valid())
            {
                struct stat st{};
                if (fstat(fd.get(), &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0 && st.st_size <= 16 * 1024 * 1024)
                {
                    std::vector<std::uint8_t> anim_data(static_cast<std::size_t>(st.st_size));
                    std::size_t total_read = 0;
                    bool read_ok = true;
                    while (total_read < anim_data.size())
                    {
                        ssize_t r = read(fd.get(), anim_data.data() + total_read, anim_data.size() - total_read);
                        if (r < 0)
                        {
                            if (errno == EINTR) continue;
                            read_ok = false;
                            break;
                        }
                        if (r == 0) break;
                        total_read += static_cast<std::size_t>(r);
                    }
                    if (read_ok && total_read == anim_data.size())
                    {
                        send_response(client_fd, 200, "OK", "image/gif", anim_data.data(), anim_data.size(),
                                      "Cache-Control: public, max-age=86400\r\n");
                        return;
                    }
                }
            }
        }
        send_string_response(client_fd, 404, "Not Found", "text/plain", "Shinchan animation not found");
        return;
    }

    // Ruta 3: API de Lista de Juegos y Partidas
    if (path == "/api/games")
    {
        auto games = get_games();
        if (games.empty())
        {
            auto scanned = scanner::scan_installed_games_and_saves();
            update_games(scanned);
            games = std::move(scanned);
        }

        std::string json = "[";
        for (std::size_t i = 0; i < games.size(); ++i)
        {
            const auto &g = games[i];
            const bool has_save = (!g.save_path.empty() && access(g.save_path.c_str(), F_OK) == 0);
            const bool has_dummy = (g.platform == "PS4") || g.has_save_dummy || has_save ||
                                   machete::save::has_local_save_dummy_for_game(g);

            if (i > 0) json += ",";
            json += "{";
            json += "\"title_id\":\"" + json_escape(g.title_id) + "\",";
            json += "\"title\":\"" + json_escape(g.title) + "\",";
            json += "\"platform\":\"" + json_escape(g.platform) + "\",";
            json += "\"save_path\":\"" + json_escape(g.save_path) + "\",";
            json += "\"has_save\":" + std::string(has_save ? "true" : "false") + ",";
            json += "\"has_save_dummy\":" + std::string(has_dummy ? "true" : "false") + ",";
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
        if (title_id.empty() || title_id.find("..") != std::string::npos ||
            title_id.find('/') != std::string::npos || title_id.find('\\') != std::string::npos)
        {
            send_string_response(client_fd, 400, "Bad Request", "text/plain", "Invalid Title ID");
            return;
        }
        std::string icon_path;
        {
            scanner::ScannedGame g;
            if (find_game(title_id, g) && !g.icon_path.empty() && access(g.icon_path.c_str(), F_OK) == 0)
            {
                icon_path = g.icon_path;
            }
        }

        if (icon_path.empty())
        {
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
            ScopedFd fd(open(icon_path.c_str(), O_RDONLY));
            if (fd.is_valid())
            {
                struct stat st{};
                if (fstat(fd.get(), &st) == 0 && S_ISREG(st.st_mode))
                {
                    if (st.st_size > 0 && st.st_size <= 16 * 1024 * 1024)
                    {
                        std::vector<std::uint8_t> icon_data(static_cast<std::size_t>(st.st_size));
                        std::size_t total_read = 0;
                        bool read_ok = true;
                        while (total_read < icon_data.size())
                        {
                            ssize_t r = read(fd.get(), icon_data.data() + total_read, icon_data.size() - total_read);
                            if (r < 0)
                            {
                                if (errno == EINTR) continue;
                                read_ok = false;
                                break;
                            }
                            if (r == 0) break;
                            total_read += static_cast<std::size_t>(r);
                        }

                        if (read_ok && total_read == icon_data.size())
                        {
                            send_response(client_fd, 200, "OK", "image/png", icon_data.data(), icon_data.size(),
                                          "Cache-Control: public, max-age=86400\r\n");
                            return;
                        }
                    }
                }
            }
        }

        send_string_response(client_fd, 404, "Not Found", "text/plain", "Icon not found");
        return;
    }

    // Ruta 5: API de Descarga Comprimida (.zip) de una Partida Individual
    if (path.rfind("/api/download/", 0) == 0)
    {
        std::string title_id = path.substr(14);
        if (title_id.empty() || title_id.find("..") != std::string::npos ||
            title_id.find('/') != std::string::npos || title_id.find('\\') != std::string::npos)
        {
            send_string_response(client_fd, 400, "Bad Request", "text/plain", "Invalid Title ID");
            return;
        }
        scanner::ScannedGame target_game;
        if (!find_game(title_id, target_game))
        {
            auto scanned = scanner::scan_installed_games_and_saves();
            update_games(scanned);
            if (!find_game(title_id, target_game))
            {
                send_string_response(client_fd, 404, "Not Found", "text/plain", "Save data not found for title");
                return;
            }
        }

        if (target_game.save_path.empty() || access(target_game.save_path.c_str(), F_OK) != 0)
        {
            send_string_response(client_fd, 404, "Not Found", "text/plain", "Save data not found for title");
            return;
        }

        ZipWriter zip;
        zip.add_directory(target_game.save_path, target_game.title_id);

        // Si es PS5 o PS4, incluir tambien metadatos (.meta / meta / sce_sys) para visibilidad en cualquier SO
        std::string meta_path = target_game.save_path;
        std::size_t pos = meta_path.find("savedata_prospero");
        if (pos != std::string::npos)
        {
            meta_path.replace(pos, 17, "savedata_prospero_meta");
            if (access(meta_path.c_str(), F_OK) == 0)
            {
                zip.add_directory(meta_path, target_game.title_id + "/.meta");
                zip.add_directory(meta_path, target_game.title_id + "/meta");
                zip.add_directory(meta_path, target_game.title_id + "/sce_sys");
            }
        }
        else
        {
            std::size_t pos4 = meta_path.find("/savedata/");
            if (pos4 != std::string::npos)
            {
                meta_path.replace(pos4, 10, "/savedata_meta/user/");
                if (access(meta_path.c_str(), F_OK) == 0)
                {
                    zip.add_directory(meta_path, target_game.title_id + "/.meta");
                    zip.add_directory(meta_path, target_game.title_id + "/meta");
                    zip.add_directory(meta_path, target_game.title_id + "/sce_sys");
                }
            }
        }

        // Boveda Criptografica SAMU: generar o vincular crypto_vault.bin
        std::string vault_path = resolve_or_create_crypto_vault(target_game);
        if (!vault_path.empty() && access(vault_path.c_str(), F_OK) == 0)
        {
            zip.add_file(vault_path, target_game.title_id + "/crypto_vault.bin");
        }

        // Archivos Desencriptados en Claro (PFS Mount): incluir decrypted/ si disponible
        std::string dec_dir = resolve_or_extract_decrypted_dir(target_game);
        if (!dec_dir.empty() && access(dec_dir.c_str(), F_OK) == 0)
        {
            zip.add_directory(dec_dir, target_game.title_id + "/decrypted");
        }

        // Keystone autentico de la consola
        std::string keystone_path = resolve_authentic_keystone(target_game);
        if (!keystone_path.empty() && access(keystone_path.c_str(), F_OK) == 0)
        {
            zip.add_file(keystone_path, target_game.title_id + "/keystone");
        }

        zip.prepare();
        if (zip.entry_count() == 0)
        {
            send_string_response(client_fd, 404, "Not Found", "text/plain", "Save folder contains no files");
            return;
        }

        std::string filename = target_game.title_id + "_" + sanitize_filename(target_game.title) + "_save.zip";
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

        if (header_len > 0 && static_cast<std::size_t>(header_len) < sizeof(header_buf))
        {
            if (send_all_socket(client_fd, header_buf, static_cast<std::size_t>(header_len)))
            {
                zip.stream_to_fd(client_fd);
            }
        }
        return;
    }

    // Ruta 6: API de Descarga Comprimida de TODAS las Partidas
    if (path == "/api/download_all")
    {
        auto games = get_games();
        if (games.empty())
        {
            auto scanned = scanner::scan_installed_games_and_saves();
            update_games(scanned);
            games = std::move(scanned);
        }

        ZipWriter zip;
        for (const auto &g : games)
        {
            if (!g.save_path.empty() && access(g.save_path.c_str(), F_OK) == 0)
            {
                std::string folder_name = g.title_id + " - " + sanitize_filename(g.title);
                zip.add_directory(g.save_path, folder_name);

                // Si es PS5 o PS4, incluir tambien metadatos (.meta / meta / sce_sys) para visibilidad en cualquier SO
                std::string meta_path = g.save_path;
                std::size_t pos = meta_path.find("savedata_prospero");
                if (pos != std::string::npos)
                {
                    meta_path.replace(pos, 17, "savedata_prospero_meta");
                    if (access(meta_path.c_str(), F_OK) == 0)
                    {
                        zip.add_directory(meta_path, folder_name + "/.meta");
                        zip.add_directory(meta_path, folder_name + "/meta");
                        zip.add_directory(meta_path, folder_name + "/sce_sys");
                    }
                }
                else
                {
                    std::size_t pos4 = meta_path.find("/savedata/");
                    if (pos4 != std::string::npos)
                    {
                        meta_path.replace(pos4, 10, "/savedata_meta/user/");
                        if (access(meta_path.c_str(), F_OK) == 0)
                        {
                            zip.add_directory(meta_path, folder_name + "/.meta");
                            zip.add_directory(meta_path, folder_name + "/meta");
                            zip.add_directory(meta_path, folder_name + "/sce_sys");
                        }
                    }
                }

                // Boveda Criptografica SAMU: generar o vincular crypto_vault.bin
                std::string vault_path = resolve_or_create_crypto_vault(g);
                if (!vault_path.empty() && access(vault_path.c_str(), F_OK) == 0)
                {
                    zip.add_file(vault_path, folder_name + "/crypto_vault.bin");
                }

                // Archivos Desencriptados en Claro (PFS Mount): incluir decrypted/ si disponible
                std::string dec_dir = resolve_or_extract_decrypted_dir(g);
                if (!dec_dir.empty() && access(dec_dir.c_str(), F_OK) == 0)
                {
                    zip.add_directory(dec_dir, folder_name + "/decrypted");
                }

                // Keystone autentico de la consola
                std::string keystone_path = resolve_authentic_keystone(g);
                if (!keystone_path.empty() && access(keystone_path.c_str(), F_OK) == 0)
                {
                    zip.add_file(keystone_path, folder_name + "/keystone");
                }
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

        if (header_len > 0 && static_cast<std::size_t>(header_len) < sizeof(header_buf))
        {
            if (send_all_socket(client_fd, header_buf, static_cast<std::size_t>(header_len)))
            {
                zip.stream_to_fd(client_fd);
            }
        }
        return;
    }

    // Ruta 6B: API de Listado de Copias de Seguridad Disponibles en la Consola
    if (path.rfind("/api/backups/", 0) == 0)
    {
        std::string title_id = path.substr(13);
        if (title_id.empty() || title_id.find("..") != std::string::npos ||
            title_id.find('/') != std::string::npos || title_id.find('\\') != std::string::npos)
        {
            send_string_response(client_fd, 400, "Bad Request", "application/json", "[]");
            return;
        }

        scanner::ScannedGame target_game;
        if (!find_game(title_id, target_game))
        {
            target_game.title_id = title_id;
            target_game.title = title_id;
            target_game.platform = (title_id.rfind("CUSA", 0) == 0) ? "PS4" : "PS5";
        }

        auto devs = machete::storage::scan_storage_devices();
        auto backups = machete::save::find_available_backups(target_game, devs);
        std::string json = "[";
        for (std::size_t i = 0; i < backups.size(); ++i)
        {
            const auto &b = backups[i];
            if (i > 0) json += ",";
            json += "{";
            json += "\"index\":" + std::to_string(i) + ",";
            json += "\"device_label\":\"" + json_escape(b.device_label) + "\",";
            json += "\"folder_name\":\"" + json_escape(b.folder_name) + "\",";
            json += "\"backup_path\":\"" + json_escape(b.folder_path) + "\",";
            json += "\"timestamp_str\":\"" + json_escape(b.timestamp_str) + "\",";
            json += "\"size_str\":\"" + json_escape(b.size_str) + "\",";
            json += "\"file_count\":" + std::to_string(b.file_count) + ",";
            json += "\"has_keystone\":" + std::string(b.has_keystone ? "true" : "false") + ",";
            json += "\"has_decrypted\":" + std::string(b.has_decrypted ? "true" : "false") + ",";
            json += "\"has_crypto_vault\":" + std::string(b.has_crypto_vault ? "true" : "false");
            json += "}";
        }
        json += "]";
        send_string_response(client_fd, 200, "OK", "application/json", json);
        return;
    }

    // Ruta 6C: API para Crear Respaldo en Consola desde el Navegador Web
    if (path.rfind("/api/backup/", 0) == 0 && method_str == "POST")
    {
        std::string title_id = path.substr(12);
        if (title_id.empty() || title_id.find("..") != std::string::npos ||
            title_id.find('/') != std::string::npos || title_id.find('\\') != std::string::npos)
        {
            send_string_response(client_fd, 400, "Bad Request", "application/json",
                                 "{\"success\":false,\"message\":\"Title ID invalido\"}");
            return;
        }

        scanner::ScannedGame target_game;
        if (!find_game(title_id, target_game))
        {
            auto scanned = scanner::scan_installed_games_and_saves();
            update_games(scanned);
            if (!find_game(title_id, target_game))
            {
                send_string_response(client_fd, 404, "Not Found", "application/json",
                                     "{\"success\":false,\"message\":\"Juego no encontrado en la consola\"}");
                return;
            }
        }

        machete::storage::ensure_internal_storage();
        auto devs = machete::storage::scan_storage_devices();
        machete::storage::StorageDevice target_dev{
            machete::storage::StorageType::Internal,
            "internal",
            "Almacenamiento Interno SSD",
            "/data",
            "/data/MacheteSVG",
            true,
            1024ULL * 1024ULL * 1024ULL,
            1024ULL * 1024ULL * 1024ULL};
        for (const auto &d : devs)
        {
            if (d.connected)
            {
                target_dev = d;
                break;
            }
        }

        auto res = machete::save::backup_game_save(target_game, target_dev);
        std::string json = "{\"success\":" + std::string(res.success ? "true" : "false") +
                           ",\"message\":\"" + json_escape(res.message) + "\"}";
        send_string_response(client_fd, 200, "OK", "application/json", json);
        return;
    }

    // Ruta 6D: API para Restaurar / Resignar (Apollo PS4 & Cross-Console PS5 Save Dummy) desde el Navegador Web
    if (path.rfind("/api/restore/", 0) == 0 && method_str == "POST")
    {
        std::string title_id = path.substr(13);
        if (title_id.empty() || title_id.find("..") != std::string::npos ||
            title_id.find('/') != std::string::npos || title_id.find('\\') != std::string::npos)
        {
            send_string_response(client_fd, 400, "Bad Request", "application/json",
                                 "{\"success\":false,\"message\":\"Title ID invalido\"}");
            return;
        }

        std::size_t idx = 0;
        if (query_pos != std::string::npos)
        {
            std::string qs = decoded_path.substr(query_pos + 1);
            std::size_t ipos = qs.find("index=");
            if (ipos != std::string::npos)
            {
                idx = static_cast<std::size_t>(std::strtoul(qs.c_str() + ipos + 6, nullptr, 10));
            }
        }

        scanner::ScannedGame target_game;
        if (!find_game(title_id, target_game))
        {
            auto scanned = scanner::scan_installed_games_and_saves();
            update_games(scanned);
            if (!find_game(title_id, target_game))
            {
                target_game.title_id = title_id;
                target_game.title = title_id;
                target_game.platform = (title_id.rfind("CUSA", 0) == 0) ? "PS4" : "PS5";
            }
        }

        auto devs = machete::storage::scan_storage_devices();
        auto backups = machete::save::find_available_backups(target_game, devs);
        if (backups.empty() || idx >= backups.size())
        {
            send_string_response(client_fd, 404, "Not Found", "application/json",
                                 "{\"success\":false,\"message\":\"No se encontro la copia de seguridad solicitada en /data/MacheteSVG ni en USB\"}");
            return;
        }

        auto res = machete::save::restore_backup_item(target_game, backups[idx]);
        if (res.success)
        {
            auto scanned = scanner::scan_installed_games_and_saves();
            update_games(scanned);
        }

        std::string json = "{\"success\":" + std::string(res.success ? "true" : "false") +
                           ",\"message\":\"" + json_escape(res.message) + "\"}";
        send_string_response(client_fd, 200, "OK", "application/json", json);
        return;
    }

    // Ruta 6E: API para Respaldo en Lote (Batch Backup)
    if (path == "/api/batch_backup" && method_str == "POST")
    {
        std::string body;
        const char *hdr_end = std::strstr(req_buf, "\r\n\r\n");
        std::size_t hdr_len = 0;
        if (hdr_end)
            hdr_len = static_cast<std::size_t>((hdr_end - req_buf) + 4);
        else
        {
            hdr_end = std::strstr(req_buf, "\n\n");
            if (hdr_end)
                hdr_len = static_cast<std::size_t>((hdr_end - req_buf) + 2);
        }

        std::size_t content_length = 0;
        if (hdr_len > 0)
        {
            std::string raw_hdrs(req_buf, hdr_len);
            std::size_t cl_pos = raw_hdrs.find("Content-Length:");
            if (cl_pos == std::string::npos)
                cl_pos = raw_hdrs.find("content-length:");
            if (cl_pos != std::string::npos)
            {
                cl_pos += 15;
                while (cl_pos < hdr_len && (raw_hdrs[cl_pos] == ' ' || raw_hdrs[cl_pos] == '\t'))
                    cl_pos++;
                content_length = static_cast<std::size_t>(std::strtoul(raw_hdrs.c_str() + cl_pos, nullptr, 10));
            }

            if (content_length > 1024 * 1024)
            {
                send_string_response(client_fd, 400, "Bad Request", "application/json",
                                     "{\"success\":false,\"message\":\"Tamano de solicitud demasiado grande (maximo 1 MB)\"}");
                return;
            }

            const char *body_start = req_buf + hdr_len;
            std::size_t body_in_req = (static_cast<std::size_t>(n) > hdr_len)
                                          ? static_cast<std::size_t>(n) - hdr_len
                                          : 0;
            body.append(body_start, body_in_req);

            if (content_length > body_in_req)
            {
                std::size_t rem = content_length - body_in_req;
                while (rem > 0)
                {
                    char chunk[2048];
                    std::size_t to_read = std::min(sizeof(chunk), rem);
                    ssize_t r = recv(client_fd, chunk, to_read, 0);
                    if (r <= 0)
                        break;
                    body.append(chunk, static_cast<std::size_t>(r));
                    rem -= static_cast<std::size_t>(r);
                }
            }
        }

        auto parsed = parse_batch_json_payload(body);
        if (!parsed.valid)
        {
            send_string_response(client_fd, 400, "Bad Request", "application/json",
                                 "{\"success\":false,\"message\":\"" + json_escape(parsed.error_message) + "\"}");
            return;
        }

        std::vector<std::string> title_ids = std::move(parsed.title_ids);
        std::string dest_root = parsed.custom_root;

        if (dest_root.empty() && query_pos != std::string::npos)
        {
            std::string qs = decoded_path.substr(query_pos + 1);
            std::size_t rpos = qs.find("root=");
            if (rpos != std::string::npos)
            {
                std::size_t amp = qs.find('&', rpos);
                dest_root = (amp == std::string::npos) ? qs.substr(rpos + 5) : qs.substr(rpos + 5, amp - (rpos + 5));
            }
        }
        if (dest_root.empty())
        {
            machete::storage::ensure_internal_storage();
            dest_root = "/data/" + std::string(storage::ROOT_DIR_NAME);
        }

        auto summary = machete::save::execute_batch_backup(title_ids, dest_root);
        std::string json = "{";
        json += "\"total_items\":" + std::to_string(summary.total_items) + ",";
        json += "\"succeeded\":" + std::to_string(summary.succeeded) + ",";
        json += "\"failed\":" + std::to_string(summary.failed) + ",";
        json += "\"total_bytes\":" + std::to_string(summary.total_bytes) + ",";
        json += "\"results\":[";
        for (std::size_t i = 0; i < summary.results.size(); ++i)
        {
            const auto &r = summary.results[i];
            if (i > 0)
                json += ",";
            json += "{";
            json += "\"title_id\":\"" + json_escape(r.title_id) + "\",";
            json += "\"title_name\":\"" + json_escape(r.title_name) + "\",";
            json += "\"success\":" + std::string(r.success ? "true" : "false") + ",";
            json += "\"error_message\":\"" + json_escape(r.error_message) + "\",";
            json += "\"bytes_transferred\":" + std::to_string(r.bytes_transferred);
            json += "}";
        }
        json += "]}";
        send_string_response(client_fd, 200, "OK", "application/json", json);
        return;
    }

    // Ruta 6F: API para Restauracion en Lote (Batch Restore)
    if (path == "/api/batch_restore" && method_str == "POST")
    {
        std::string body;
        const char *hdr_end = std::strstr(req_buf, "\r\n\r\n");
        std::size_t hdr_len = 0;
        if (hdr_end)
            hdr_len = static_cast<std::size_t>((hdr_end - req_buf) + 4);
        else
        {
            hdr_end = std::strstr(req_buf, "\n\n");
            if (hdr_end)
                hdr_len = static_cast<std::size_t>((hdr_end - req_buf) + 2);
        }

        std::size_t content_length = 0;
        if (hdr_len > 0)
        {
            std::string raw_hdrs(req_buf, hdr_len);
            std::size_t cl_pos = raw_hdrs.find("Content-Length:");
            if (cl_pos == std::string::npos)
                cl_pos = raw_hdrs.find("content-length:");
            if (cl_pos != std::string::npos)
            {
                cl_pos += 15;
                while (cl_pos < hdr_len && (raw_hdrs[cl_pos] == ' ' || raw_hdrs[cl_pos] == '\t'))
                    cl_pos++;
                content_length = static_cast<std::size_t>(std::strtoul(raw_hdrs.c_str() + cl_pos, nullptr, 10));
            }

            if (content_length > 1024 * 1024)
            {
                send_string_response(client_fd, 400, "Bad Request", "application/json",
                                     "{\"success\":false,\"message\":\"Tamano de solicitud demasiado grande (maximo 1 MB)\"}");
                return;
            }

            const char *body_start = req_buf + hdr_len;
            std::size_t body_in_req = (static_cast<std::size_t>(n) > hdr_len)
                                          ? static_cast<std::size_t>(n) - hdr_len
                                          : 0;
            body.append(body_start, body_in_req);

            if (content_length > body_in_req)
            {
                std::size_t rem = content_length - body_in_req;
                while (rem > 0)
                {
                    char chunk[2048];
                    std::size_t to_read = std::min(sizeof(chunk), rem);
                    ssize_t r = recv(client_fd, chunk, to_read, 0);
                    if (r <= 0)
                        break;
                    body.append(chunk, static_cast<std::size_t>(r));
                    rem -= static_cast<std::size_t>(r);
                }
            }
        }

        auto parsed = parse_batch_json_payload(body);
        if (!parsed.valid)
        {
            send_string_response(client_fd, 400, "Bad Request", "application/json",
                                 "{\"success\":false,\"message\":\"" + json_escape(parsed.error_message) + "\"}");
            return;
        }

        std::vector<std::string> title_ids = std::move(parsed.title_ids);
        std::string backup_root = parsed.custom_root;

        if (backup_root.empty() && query_pos != std::string::npos)
        {
            std::string qs = decoded_path.substr(query_pos + 1);
            std::size_t rpos = qs.find("root=");
            if (rpos != std::string::npos)
            {
                std::size_t amp = qs.find('&', rpos);
                backup_root = (amp == std::string::npos) ? qs.substr(rpos + 5) : qs.substr(rpos + 5, amp - (rpos + 5));
            }
        }
        if (backup_root.empty())
        {
            machete::storage::ensure_internal_storage();
            backup_root = "/data/" + std::string(storage::ROOT_DIR_NAME);
        }

        auto summary = machete::save::execute_batch_restore(title_ids, backup_root);
        if (summary.succeeded > 0)
        {
            auto scanned = scanner::scan_installed_games_and_saves();
            update_games(scanned);
        }

        std::string json = "{";
        json += "\"total_items\":" + std::to_string(summary.total_items) + ",";
        json += "\"succeeded\":" + std::to_string(summary.succeeded) + ",";
        json += "\"failed\":" + std::to_string(summary.failed) + ",";
        json += "\"total_bytes\":" + std::to_string(summary.total_bytes) + ",";
        json += "\"results\":[";
        for (std::size_t i = 0; i < summary.results.size(); ++i)
        {
            const auto &r = summary.results[i];
            if (i > 0)
                json += ",";
            json += "{";
            json += "\"title_id\":\"" + json_escape(r.title_id) + "\",";
            json += "\"title_name\":\"" + json_escape(r.title_name) + "\",";
            json += "\"success\":" + std::string(r.success ? "true" : "false") + ",";
            json += "\"error_message\":\"" + json_escape(r.error_message) + "\",";
            json += "\"bytes_transferred\":" + std::to_string(r.bytes_transferred);
            json += "}";
        }
        json += "]}";
        send_string_response(client_fd, 200, "OK", "application/json", json);
        return;
    }

    // Ruta 6G: API de Actualizacion Remota de MacheteSVG (Subida de ZIP via Web OTA)
    if (path == "/api/update" && method_str == "POST")
    {
        if (!is_update_module_enabled())
        {
            send_string_response(client_fd, 403, "Forbidden", "application/json",
                                 "{\"success\":false,\"message\":\"El modulo de actualizacion remota esta deshabilitado en esta version.\"}");
            return;
        }

        MACHETE_LOG("[OTA_UPDATE]", "Received OTA update request from client");

        // 1. Localizar fin de cabeceras HTTP (\r\n\r\n o \n\n)
        const char *hdr_end = std::strstr(req_buf, "\r\n\r\n");
        std::size_t header_len = 0;
        if (hdr_end)
        {
            header_len = static_cast<std::size_t>((hdr_end - req_buf) + 4);
        }
        else
        {
            hdr_end = std::strstr(req_buf, "\n\n");
            if (hdr_end)
            {
                header_len = static_cast<std::size_t>((hdr_end - req_buf) + 2);
            }
        }

        if (header_len == 0)
        {
            send_string_response(client_fd, 400, "Bad Request", "application/json",
                                 "{\"success\":false,\"message\":\"Cabeceras HTTP invalidas\"}");
            return;
        }

        // 2. Extraer Content-Length
        std::string raw_headers(req_buf, header_len);
        std::uint64_t content_length = 0;
        std::size_t cl_pos = raw_headers.find("Content-Length:");
        if (cl_pos == std::string::npos)
        {
            cl_pos = raw_headers.find("content-length:");
        }
        if (cl_pos != std::string::npos)
        {
            cl_pos += 15;
            while (cl_pos < header_len && (raw_headers[cl_pos] == ' ' || raw_headers[cl_pos] == '\t'))
            {
                cl_pos++;
            }
            content_length = std::strtoull(raw_headers.c_str() + cl_pos, nullptr, 10);
        }

        // Limite de seguridad: entre 22 bytes (EOCD minimo) y 500 MB
        if (content_length < 22 || content_length > 500ULL * 1024ULL * 1024ULL)
        {
            send_string_response(client_fd, 400, "Bad Request", "application/json",
                                 "{\"success\":false,\"message\":\"Tamano de paquete no valido o cabecera Content-Length ausente\"}");
            return;
        }

        // 3. Preparar directorio temporal de recepcion
        std::string staging_dir = "/data/homebrew/.update_staging";
        if (access("/data/homebrew", W_OK) != 0)
        {
            staging_dir = "/data/MacheteSVG/update_staging";
            if (access("/data", W_OK) != 0)
            {
                staging_dir = "/tmp/machete_update_staging";
            }
        }
        make_directory_recursive(staging_dir);
        std::string staging_zip = staging_dir + "/update.zip";
        unlink(staging_zip.c_str());

        ScopedFd out_fd(open(staging_zip.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0777));
        if (!out_fd.is_valid())
        {
            send_string_response(client_fd, 500, "Internal Server Error", "application/json",
                                 "{\"success\":false,\"message\":\"No se pudo crear el archivo temporal de actualizacion\"}");
            return;
        }

        // 4. Escribir los bytes iniciales del cuerpo recibidos en el primer buffer
        std::size_t initial_bytes = static_cast<std::size_t>(n) - header_len;
        if (initial_bytes > 0)
        {
            std::size_t to_write = std::min<std::uint64_t>(initial_bytes, content_length);
            if (write(out_fd.get(), req_buf + header_len, to_write) != static_cast<ssize_t>(to_write))
            {
                out_fd.reset();
                unlink(staging_zip.c_str());
                send_string_response(client_fd, 500, "Internal Server Error", "application/json",
                                     "{\"success\":false,\"message\":\"Fallo de escritura en almacenamiento SSD\"}");
                return;
            }
        }

        // 5. Recibir el resto del flujo por bloques de 64 KB
        std::uint64_t bytes_received = initial_bytes;
        constexpr std::size_t kChunkSize = 64 * 1024;
        std::vector<std::uint8_t> chunk(kChunkSize);
        bool receive_ok = true;

        while (bytes_received < content_length)
        {
            std::size_t to_read = std::min<std::uint64_t>(content_length - bytes_received, chunk.size());
            ssize_t r = recv(client_fd, chunk.data(), to_read, 0);
            if (r <= 0)
            {
                if (r < 0 && (errno == EINTR || errno == EAGAIN))
                {
                    continue;
                }
                receive_ok = false;
                break;
            }

            ssize_t w = write(out_fd.get(), chunk.data(), static_cast<std::size_t>(r));
            if (w != r)
            {
                receive_ok = false;
                break;
            }
            bytes_received += static_cast<std::uint64_t>(r);
        }

        out_fd.reset(); // Garantiza el cierre del descriptor antes de abrir con ZipReader

        if (!receive_ok || bytes_received < content_length)
        {
            unlink(staging_zip.c_str());
            send_string_response(client_fd, 400, "Bad Request", "application/json",
                                 "{\"success\":false,\"message\":\"Conexion interrumpida durante la subida del paquete ZIP\"}");
            return;
        }

        MACHETE_LOG("[OTA_UPDATE]", "Uploaded package received: %llu bytes",
                    static_cast<unsigned long long>(bytes_received));

        // 6. Si fue enviado via multipart/form-data, detectar si hay cabecera preliminar y localizar PK\x03\x04
        {
            ScopedFd check_fd(open(staging_zip.c_str(), O_RDONLY));
            if (check_fd.is_valid())
            {
                std::uint8_t magic[4] = {0};
                if (read(check_fd.get(), magic, 4) == 4)
                {
                    if (magic[0] != 0x50 || magic[1] != 0x4B)
                    {
                        lseek(check_fd.get(), 0, SEEK_SET);
                        std::vector<std::uint8_t> head(8192);
                        ssize_t hr = read(check_fd.get(), head.data(), head.size());
                        if (hr > 4)
                        {
                            off_t pk_offset = -1;
                            for (ssize_t i = 0; i + 4 <= hr; ++i)
                            {
                                if (head[i] == 0x50 && head[i+1] == 0x4B && head[i+2] == 0x03 && head[i+3] == 0x04)
                                {
                                    pk_offset = i;
                                    break;
                                }
                            }
                            if (pk_offset > 0)
                            {
                                check_fd.reset();
                                std::string clean_zip = staging_dir + "/update_clean.zip";
                                unlink(clean_zip.c_str());
                                ScopedFd src_fd(open(staging_zip.c_str(), O_RDONLY));
                                ScopedFd dst_fd(open(clean_zip.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0777));
                                if (src_fd.is_valid() && dst_fd.is_valid())
                                {
                                    lseek(src_fd.get(), pk_offset, SEEK_SET);
                                    std::vector<std::uint8_t> copy_buf(64 * 1024);
                                    ssize_t cr;
                                    while ((cr = read(src_fd.get(), copy_buf.data(), copy_buf.size())) > 0)
                                    {
                                        write(dst_fd.get(), copy_buf.data(), cr);
                                    }
                                    src_fd.reset();
                                    dst_fd.reset();
                                    unlink(staging_zip.c_str());
                                    rename(clean_zip.c_str(), staging_zip.c_str());
                                }
                            }
                        }
                    }
                }
            }
        }

        // 7. Abrir y validar estructura del paquete ZIP
        ZipReader reader;
        if (!reader.open_file(staging_zip))
        {
            unlink(staging_zip.c_str());
            send_string_response(client_fd, 400, "Bad Request", "application/json",
                                 "{\"success\":false,\"message\":\"El archivo recibido no es un archivo ZIP valido o esta dañado\"}");
            return;
        }

        // Validar autenticidad: debe contener al menos eboot.bin o param.json
        if (!reader.has_file("eboot.bin") && !reader.has_file("param.json"))
        {
            reader.close();
            unlink(staging_zip.c_str());
            send_string_response(client_fd, 400, "Bad Request", "application/json",
                                 "{\"success\":false,\"message\":\"El archivo ZIP no contiene un paquete valido de MacheteSVG (falta eboot.bin)\"}");
            return;
        }

        // 8. Determinar directorio de instalacion (/data/homebrew/PPSA99700)
        std::string target_install_dir = "/data/homebrew/PPSA99700";
        if (access("/data/homebrew", W_OK) != 0 && access("/data", W_OK) != 0)
        {
            // Fallback para entornos de pruebas o emulacion
            target_install_dir = "/tmp/machete_test_homebrew/PPSA99700";
        }
        make_directory_recursive(target_install_dir);

        // 9. Extraer y reemplazar todos los archivos, con depuracion de archivos obsoletos
        auto res = reader.extract_all(target_install_dir, true, true);
        reader.close();

        // 10. Limpiar archivo de subida y directorio de staging
        unlink(staging_zip.c_str());
        rmdir(staging_dir.c_str());

        if (!res.success)
        {
            send_string_response(client_fd, 500, "Internal Server Error", "application/json",
                                 "{\"success\":false,\"message\":\"" + json_escape(res.error_message) + "\"}");
            return;
        }

        // 11. Reforzar permisos 0777 en ejecutables y modulos criticos
        chmod((target_install_dir + "/eboot.bin").c_str(), 0777);
        chmod((target_install_dir + "/sce_module/libc.prx").c_str(), 0777);
        chmod((target_install_dir + "/daemon/lapy_jb_daemon.elf").c_str(), 0777);

        // 12. Reiniciar el archivo de registro machete.log para garantizar un registro 100% fresco
        machete::log::reset_fresh();

        // Sincronizar almacenamiento SSD NVMe
        sync();

        MACHETE_LOG("[OTA_UPDATE]", "Clean update deployed: %zu extracted (%llu bytes), %zu obsolete removed -> %s",
                    res.files_extracted, static_cast<unsigned long long>(res.total_bytes_extracted),
                    res.files_cleaned, target_install_dir.c_str());

        std::string json = "{\"success\":true,\"message\":\"Actualizacion limpia instalada con exito. Se extrajeron " +
                           std::to_string(res.files_extracted) + " archivos (" +
                           std::to_string(res.total_bytes_extracted / (1024 * 1024)) + " MB) y se depuraron " +
                           std::to_string(res.files_cleaned) + " archivos obsoletos en " +
                           json_escape(target_install_dir) +
                           ". El registro machete.log ha sido reiniciado. Todo ha quedado limpio como via FTP. Por favor, reinicia la aplicacion en tu consola.\","
                           "\"files_updated\":" + std::to_string(res.files_extracted) +
                           ",\"files_cleaned\":" + std::to_string(res.files_cleaned) +
                           ",\"total_bytes\":" + std::to_string(res.total_bytes_extracted) + "}";

        send_string_response(client_fd, 200, "OK", "application/json", json);
        return;
    }

    // Ruta 7: API de Exploracion de Archivos Individuales de una Partida
    if (path.rfind("/api/files/", 0) == 0)
    {
        std::string title_id = path.substr(11);
        if (title_id.empty() || title_id.find("..") != std::string::npos ||
            title_id.find('/') != std::string::npos || title_id.find('\\') != std::string::npos)
        {
            send_string_response(client_fd, 400, "Bad Request", "application/json", "[]");
            return;
        }
        scanner::ScannedGame target_game;
        if (!find_game(title_id, target_game))
        {
            auto scanned = scanner::scan_installed_games_and_saves();
            update_games(scanned);
            if (!find_game(title_id, target_game))
            {
                send_string_response(client_fd, 404, "Not Found", "application/json", "[]");
                return;
            }
        }

        if (target_game.save_path.empty() || access(target_game.save_path.c_str(), F_OK) != 0)
        {
            send_string_response(client_fd, 404, "Not Found", "application/json", "[]");
            return;
        }

        ScopedDir dir(opendir(target_game.save_path.c_str()));
        if (!dir.is_valid())
        {
            send_string_response(client_fd, 404, "Not Found", "application/json", "[]");
            return;
        }

        std::string json = "[";
        bool first = true;
        struct dirent *de = nullptr;
        while ((de = readdir(dir.get())) != nullptr)
        {
            if (std::strcmp(de->d_name, ".") == 0 || std::strcmp(de->d_name, "..") == 0)
            {
                continue;
            }

            std::string full_path = target_game.save_path + "/" + de->d_name;
            struct stat st{};
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

        // Comprobar y agregar keystone si no estaba dentro de save_path
        std::string keystone_path = "/user/appmeta/" + target_game.title_id + "/sce_sys/keystone";
        struct stat st_key{};
        if (stat(keystone_path.c_str(), &st_key) == 0 && S_ISREG(st_key.st_mode))
        {
            if (!first) json += ",";
            first = false;
            json += "{\"name\":\"keystone\",\"size_bytes\":" + std::to_string(st_key.st_size) +
                    ",\"size_formatted\":\"" + format_bytes(static_cast<std::uint64_t>(st_key.st_size)) + "\"}";
        }

        // Comprobar y agregar crypto_vault.bin
        std::string vault_path = resolve_or_create_crypto_vault(target_game);
        struct stat st_vault{};
        if (!vault_path.empty() && stat(vault_path.c_str(), &st_vault) == 0 && S_ISREG(st_vault.st_mode))
        {
            if (!first) json += ",";
            first = false;
            json += "{\"name\":\"crypto_vault.bin\",\"size_bytes\":" + std::to_string(st_vault.st_size) +
                    ",\"size_formatted\":\"" + format_bytes(static_cast<std::uint64_t>(st_vault.st_size)) + "\"}";
        }

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

        // Seguridad: Prohibir directory traversal y caracteres peligrosos
        if (filename.find("..") != std::string::npos || filename.find('/') != std::string::npos ||
            filename.find('\\') != std::string::npos || filename.find('\0') != std::string::npos)
        {
            send_string_response(client_fd, 403, "Forbidden", "text/plain", "Access denied");
            return;
        }

        scanner::ScannedGame target_game;
        if (!find_game(title_id, target_game))
        {
            auto scanned = scanner::scan_installed_games_and_saves();
            update_games(scanned);
            if (!find_game(title_id, target_game))
            {
                send_string_response(client_fd, 404, "Not Found", "text/plain", "Save data not found");
                return;
            }
        }

        if (filename == "crypto_vault.bin")
        {
            std::string vault_file = resolve_or_create_crypto_vault(target_game);
            if (!vault_file.empty() && access(vault_file.c_str(), F_OK) == 0)
            {
                send_file_response(client_fd, vault_file, "crypto_vault.bin");
                return;
            }
        }

        if (filename == "keystone")
        {
            std::string keystone_file = "/user/appmeta/" + target_game.title_id + "/sce_sys/keystone";
            if (access(keystone_file.c_str(), F_OK) == 0)
            {
                send_file_response(client_fd, keystone_file, "keystone");
                return;
            }
        }

        if (target_game.save_path.empty())
        {
            send_string_response(client_fd, 404, "Not Found", "text/plain", "Save data not found");
            return;
        }

        std::string target_file = target_game.save_path + "/" + filename;
        send_file_response(client_fd, target_file, filename);
        return;
    }

    send_string_response(client_fd, 404, "Not Found", "text/plain", "Endpoint not found");
}

} // namespace machete::server
