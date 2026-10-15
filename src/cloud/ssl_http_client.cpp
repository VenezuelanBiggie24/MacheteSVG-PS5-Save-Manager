// MacheteSVG - Native PS5 BSD Socket + libSceSsl Direct TLS Client Implementation
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cloud/ssl_http_client.hpp"
#include "core/logger.hpp"

#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#if defined(__PROSPERO__) || defined(__Prospero__) || defined(__PS5__)
extern "C"
{
    int sceNetInit(void);
    int sceSslInit(std::size_t size);
    int sceSslTerm(void);
    int sceSslCreateConnection(int sslCtxId, int sockId, const char *hostname);
    int sceSslDeleteConnection(int connId);
    int sceSslConnect(int connId);
    int sceSslClose(int connId);
    int sceSslSend(int connId, const void *data, std::size_t size, int flags);
    int sceSslRecv(int connId, void *data, std::size_t size, int flags);
    int sceSslDisableOption(int connId, std::uint32_t options);
    int sceSslDisableVerifyOption(int connId, std::uint32_t options);
    int sceSslSetVerifyCallback(int connId, int (*cb)(int connId, void *user_arg), void *user_arg);
}
#endif

namespace machete::cloud
{

static int s_sharedSslCtx = -1;

void SslHttpClient::set_ssl_context(int ctx)
{
    s_sharedSslCtx = ctx;
}

int SslHttpClient::get_ssl_context()
{
    return s_sharedSslCtx;
}

SslHttpResponse SslHttpClient::request(
    const std::string &target_ip,
    const std::string &hostname,
    int port,
    const std::string &method,
    const std::string &path,
    const std::string &body_data,
    const std::vector<std::pair<std::string, std::string>> &headers,
    int timeout_seconds)
{
    SslHttpResponse resp;

#if defined(__PROSPERO__) || defined(__Prospero__) || defined(__PS5__)
    int active_ctx = s_sharedSslCtx;
    if (active_ctx < 0)
    {
        (void)sceNetInit();
        active_ctx = sceSslInit(512 * 1024);
        if (active_ctx < 0)
        {
            MACHETE_LOG("[SSL-HTTP]", "sceSslInit returned: %d (0x%x)",
                        active_ctx, static_cast<unsigned int>(active_ctx));
        }
        else
        {
            s_sharedSslCtx = active_ctx;
        }
    }

    if (active_ctx < 0)
    {
        resp.error = "Error: no valid SSL context (ctx=" + std::to_string(active_ctx) + ")";
        MACHETE_LOG("[SSL-HTTP]", "%s", resp.error.c_str());
        return resp;
    }

    // 1. Crear socket BSD TCP estándar
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0)
    {
        resp.error = "Native BSD TCP socket() failed";
        return resp;
    }

    struct timeval tv{};
    tv.tv_sec = timeout_seconds;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    // 2. Conectar directamente a la dirección IP obtenida por el DNS autónomo
    struct sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_port = htons(static_cast<std::uint16_t>(port));
    if (inet_pton(AF_INET, target_ip.c_str(), &sin.sin_addr) <= 0)
    {
        close(sock);
        resp.error = "Invalid IP address: " + target_ip;
        return resp;
    }

    MACHETE_LOG("[SSL-HTTP]", "Connecting TCP to %s:%d (Host: %s)...",
                target_ip.c_str(), port, hostname.c_str());
    if (connect(sock, reinterpret_cast<struct sockaddr *>(&sin), sizeof(sin)) < 0)
    {
        close(sock);
        resp.error = "TCP connect() failed to " + target_ip;
        return resp;
    }

    // 3. Envolver el socket con libSceSsl pasando el nombre de host para TLS SNI legítimo
    int sslConn = sceSslCreateConnection(active_ctx, sock, hostname.c_str());
    if (sslConn < 0)
    {
        close(sock);
        resp.error = "sceSslCreateConnection failed (" + std::to_string(sslConn) + ")";
        return resp;
    }

    // Deshabilitar verificacion estricta y registrar callback permisivo para Google APIs
    (void)sceSslDisableOption(sslConn, 0xFFFFFFFF);
    (void)sceSslDisableVerifyOption(sslConn, 0xFFFFFFFF);
    (void)sceSslSetVerifyCallback(sslConn, [](int, void *) -> int { return 0; }, nullptr);

    // 4. Apretar manos TLS (Handshake con SNI = hostname)
    int sslRes = sceSslConnect(sslConn);
    if (sslRes < 0)
    {
        MACHETE_LOG("[SSL-HTTP]", "sceSslConnect handshake failed: %d (0x%x)", sslRes, static_cast<unsigned int>(sslRes));
        sceSslDeleteConnection(sslConn);
        close(sock);
        resp.error = "TLS handshake sceSslConnect failed (" + std::to_string(sslRes) + ")";
        return resp;
    }

    // 5. Construir petición HTTP/1.1 canónica
    std::string req = method + " " + path + " HTTP/1.1\r\n";
    req += "Host: " + hostname + "\r\n";
    req += "User-Agent: MacheteSVG-PS5/1.0.0\r\n";
    req += "Connection: close\r\n";

    bool has_content_type = false;
    for (const auto &h : headers)
    {
        if (strcasecmp(h.first.c_str(), "Content-Type") == 0)
            has_content_type = true;
        req += h.first + ": " + h.second + "\r\n";
    }

    if (method == "POST")
    {
        if (!has_content_type)
        {
            req += "Content-Type: application/x-www-form-urlencoded\r\n";
        }
        req += "Content-Length: " + std::to_string(body_data.size()) + "\r\n";
    }
    req += "\r\n";
    if (!body_data.empty())
    {
        req += body_data;
    }

    // 6. Enviar petición HTTP
    std::size_t total_sent = 0;
    while (total_sent < req.size())
    {
        int sent = sceSslSend(sslConn, req.data() + total_sent, req.size() - total_sent, 0);
        if (sent <= 0)
            break;
        total_sent += static_cast<std::size_t>(sent);
    }

    // 7. Recibir respuesta
    std::string raw_response;
    char chunk[8192];
    for (;;)
    {
        int read_bytes = sceSslRecv(sslConn, chunk, sizeof(chunk), 0);
        if (read_bytes <= 0)
            break;
        raw_response.append(chunk, static_cast<std::size_t>(read_bytes));
        if (raw_response.size() > 64 * 1024 * 1024)
            break;
    }

    // Limpieza
    sceSslClose(sslConn);
    sceSslDeleteConnection(sslConn);
    close(sock);

    // 8. Parsear respuesta HTTP (Código de estado y cuerpo)
    std::size_t header_end = raw_response.find("\r\n\r\n");
    if (header_end != std::string::npos)
    {
        std::string head = raw_response.substr(0, header_end);
        resp.body = raw_response.substr(header_end + 4);

        std::size_t first_space = head.find(' ');
        if (first_space != std::string::npos && first_space + 4 <= head.size())
        {
            resp.status = std::atoi(head.substr(first_space + 1, 3).c_str());
        }

        // Si la respuesta usa Transfer-Encoding: chunked, decodificar los chunks HTTP/1.1
        std::string head_lower = head;
        for (char &c : head_lower)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        if (head_lower.find("transfer-encoding: chunked") != std::string::npos ||
            head_lower.find("transfer-encoding:chunked") != std::string::npos)
        {
            std::string decoded;
            std::size_t pos = 0;
            const std::size_t bsize = resp.body.size();
            while (pos < bsize)
            {
                std::size_t crlf = resp.body.find("\r\n", pos);
                if (crlf == std::string::npos)
                    break;
                std::string len_str = resp.body.substr(pos, crlf - pos);
                std::size_t semi = len_str.find(';');
                if (semi != std::string::npos)
                    len_str = len_str.substr(0, semi);

                char *endp = nullptr;
                unsigned long clen = std::strtoul(len_str.c_str(), &endp, 16);
                if (endp == len_str.c_str())
                    break;
                if (clen == 0)
                    break;

                pos = crlf + 2;
                if (pos + clen > bsize)
                {
                    decoded.append(resp.body.substr(pos));
                    break;
                }
                decoded.append(resp.body.substr(pos, clen));
                pos += clen;
                if (pos + 2 <= bsize && resp.body.substr(pos, 2) == "\r\n")
                    pos += 2;
            }
            if (!decoded.empty())
            {
                resp.body = std::move(decoded);
            }
        }
    }
    else
    {
        resp.error = "Respuesta HTTP malformada o incompleta";
    }

#else
    // Host mock para pruebas unitarias
    (void)target_ip;
    (void)hostname;
    (void)port;
    (void)method;
    (void)path;
    (void)body_data;
    (void)headers;
    (void)timeout_seconds;
    resp.status = 200;
    resp.body = "{\"device_code\":\"host_code\",\"user_code\":\"MCHT-TEST\",\"verification_url\":\"https://www.google.com/device\"}";
#endif

    return resp;
}

} // namespace machete::cloud
