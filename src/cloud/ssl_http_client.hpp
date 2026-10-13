// MacheteSVG - Native PS5 BSD Socket + libSceSsl Direct TLS Client
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace machete::cloud
{

struct SslHttpResponse
{
    int status = -1;
    std::string body;
    std::string error;
};

class SslHttpClient
{
  public:
    static void set_ssl_context(int ctx);
    static int get_ssl_context();

    // Realiza petición HTTPS conectando por IP directa pero transmitiendo SNI y Host: legítimos
    static SslHttpResponse request(
        const std::string &target_ip,
        const std::string &hostname,
        int port,
        const std::string &method, // "GET" o "POST"
        const std::string &path,   // ej. "/device/code"
        const std::string &body_data,
        const std::vector<std::pair<std::string, std::string>> &headers,
        int timeout_seconds = 10);
};

} // namespace machete::cloud
