// MacheteSVG - Autonomous RFC 1035 UDP DNS Resolver
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace machete::cloud
{

struct DnsCacheEntry
{
    std::string ip;
    std::chrono::steady_clock::time_point expires_at;
};

class DnsResolver
{
  public:
    static DnsResolver &instance();

    // Resuelve un dominio a dirección IPv4 directamente por socket UDP WAN (ej. "oauth2.googleapis.com")
    // Bypassea completamente proxies o resolvers locales como NanoDNS en 127.0.0.1:53
    std::string resolve(const std::string &domain, int timeout_ms = 2000);

    // Obtiene lista de IPs candidatas (IP resuelta + Anycast fallbacks para resiliencia ante bloqueo)
    std::vector<std::string> get_candidate_ips(const std::string &domain);

    // Limpia la caché interna
    void clear_cache();

  private:
    DnsResolver();
    ~DnsResolver() = default;

    std::string query_server_udp(const std::string &server_ip, const std::string &domain, int timeout_ms);
    std::vector<std::uint8_t> build_query(const std::string &domain, std::uint16_t tx_id);
    std::string parse_response(const std::uint8_t *buf, std::size_t len, std::uint16_t tx_id);

    std::mutex mutex_;
    std::unordered_map<std::string, DnsCacheEntry> cache_;
    std::vector<std::string> upstream_servers_;
};

} // namespace machete::cloud
