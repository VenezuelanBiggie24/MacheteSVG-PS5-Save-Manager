// MacheteSVG - Autonomous RFC 1035 UDP DNS Resolver Implementation
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cloud/dns_resolver.hpp"
#include "core/logger.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace machete::cloud
{

#pragma pack(push, 1)
struct DnsHeaderWire
{
    std::uint16_t id;
    std::uint16_t flags;
    std::uint16_t qdcount;
    std::uint16_t ancount;
    std::uint16_t nscount;
    std::uint16_t arcount;
};
#pragma pack(pop)

DnsResolver &DnsResolver::instance()
{
    static DnsResolver s_inst;
    return s_inst;
}

DnsResolver::DnsResolver()
{
    // Servidores DNS públicos fiables y universales
    upstream_servers_ = {
        "8.8.8.8", // Google Primary
        "1.1.1.1", // Cloudflare Primary
        "9.9.9.9", // Quad9 Primary
        "8.8.4.4", // Google Secondary
        "1.0.0.1"  // Cloudflare Secondary
    };
}

void DnsResolver::clear_cache()
{
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
}

std::vector<std::uint8_t> DnsResolver::build_query(const std::string &domain, std::uint16_t tx_id)
{
    std::vector<std::uint8_t> packet;
    packet.resize(sizeof(DnsHeaderWire));

    auto *hdr = reinterpret_cast<DnsHeaderWire *>(packet.data());
    hdr->id = htons(tx_id);
    hdr->flags = htons(0x0100); // Standard Query, Recursion Desired (RD = 1)
    hdr->qdcount = htons(1);
    hdr->ancount = 0;
    hdr->nscount = 0;
    hdr->arcount = 0;

    // Codificación QNAME por etiquetas (ej. \x06oauth2\x0agoogleapis\x03com\x00)
    std::size_t start = 0;
    while (start < domain.size())
    {
        std::size_t dot = domain.find('.', start);
        if (dot == std::string::npos)
            dot = domain.size();
        std::size_t len = dot - start;
        packet.push_back(static_cast<std::uint8_t>(len));
        for (std::size_t i = 0; i < len; ++i)
        {
            packet.push_back(static_cast<std::uint8_t>(domain[start + i]));
        }
        start = dot + 1;
    }
    packet.push_back(0); // Terminador nulo

    // QTYPE = 1 (Tipo A - IPv4)
    packet.push_back(0x00);
    packet.push_back(0x01);
    // QCLASS = 1 (Clase IN - Internet)
    packet.push_back(0x00);
    packet.push_back(0x01);

    return packet;
}

std::string DnsResolver::parse_response(const std::uint8_t *buf, std::size_t len, std::uint16_t tx_id)
{
    if (len < sizeof(DnsHeaderWire))
        return "";

    const auto *hdr = reinterpret_cast<const DnsHeaderWire *>(buf);
    if (ntohs(hdr->id) != tx_id)
        return "";

    const std::uint16_t flags = ntohs(hdr->flags);
    if ((flags & 0x000F) != 0) // RCODE != 0 (error en respuesta)
        return "";

    const std::uint16_t ancount = ntohs(hdr->ancount);
    if (ancount == 0)
        return "";

    // Saltar sección Question
    std::size_t offset = sizeof(DnsHeaderWire);
    while (offset < len && buf[offset] != 0)
    {
        offset += buf[offset] + 1;
    }
    offset += 5; // byte 0x00 + QTYPE (2) + QCLASS (2)

    std::string resolved_ip;

    // Iterar registros en Answer Section
    for (std::uint16_t i = 0; i < ancount && offset < len; ++i)
    {
        // Saltar Name (puntero comprimido 0xC0xx o secuencia de etiquetas)
        if ((buf[offset] & 0xC0) == 0xC0)
        {
            offset += 2;
        }
        else
        {
            while (offset < len && buf[offset] != 0)
            {
                offset += buf[offset] + 1;
            }
            offset += 1;
        }

        if (offset + 10 > len)
            break;

        const std::uint16_t type = (static_cast<std::uint16_t>(buf[offset]) << 8) | buf[offset + 1];
        const std::uint16_t rclass = (static_cast<std::uint16_t>(buf[offset + 2]) << 8) | buf[offset + 3];
        const std::uint16_t rdlength = (static_cast<std::uint16_t>(buf[offset + 8]) << 8) | buf[offset + 9];
        offset += 10;

        // Tipo A, Clase IN, longitud IPv4 = 4 bytes
        if (type == 1 && rclass == 1 && rdlength == 4 && offset + 4 <= len)
        {
            char ip_str[INET_ADDRSTRLEN] = {0};
            if (inet_ntop(AF_INET, &buf[offset], ip_str, sizeof(ip_str)))
            {
                resolved_ip = ip_str;
                break;
            }
        }
        offset += rdlength;
    }

    return resolved_ip;
}

std::string DnsResolver::query_server_udp(const std::string &server_ip, const std::string &domain, int timeout_ms)
{
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0)
        return "";

    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in srv{};
    srv.sin_family = AF_INET;
    srv.sin_port = htons(53);
    if (inet_pton(AF_INET, server_ip.c_str(), &srv.sin_addr) <= 0)
    {
        close(fd);
        return "";
    }

    const std::uint16_t tx_id = static_cast<std::uint16_t>(rand() & 0xFFFF);
    auto query = build_query(domain, tx_id);

    sendto(fd, query.data(), query.size(), 0, reinterpret_cast<struct sockaddr *>(&srv), sizeof(srv));

    struct pollfd pfd{};
    pfd.fd = fd;
    pfd.events = POLLIN;

    std::string result_ip;
    if (poll(&pfd, 1, timeout_ms) > 0 && (pfd.revents & POLLIN))
    {
        std::uint8_t buffer[1024];
        ssize_t recvd = recvfrom(fd, buffer, sizeof(buffer), 0, nullptr, nullptr);
        if (recvd > 0)
        {
            result_ip = parse_response(buffer, static_cast<std::size_t>(recvd), tx_id);
        }
    }

    close(fd);
    return result_ip;
}

std::string DnsResolver::resolve(const std::string &domain, int timeout_ms)
{
    const auto now = std::chrono::steady_clock::now();

    // 1. Revisar caché en memoria
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(domain);
        if (it != cache_.end() && it->second.expires_at > now)
        {
            return it->second.ip;
        }
    }

    // 2. Consulta directa en cascada a servidores públicos (bypasseando 127.0.0.1)
    for (const auto &server : upstream_servers_)
    {
        MACHETE_LOG("[DNS]", "Querying '%s' directly at %s:53...", domain.c_str(), server.c_str());
        std::string ip = query_server_udp(server, domain, timeout_ms);
        if (!ip.empty())
        {
            MACHETE_LOG("[DNS]", "Successfully resolved: %s -> %s (via %s)", domain.c_str(), ip.c_str(), server.c_str());

            std::lock_guard<std::mutex> lock(mutex_);
            cache_[domain] = {ip, now + std::chrono::seconds(300)};
            return ip;
        }
    }

    // 3. Fallback estático de Anycast IPs para dominios de Google (resiliencia si UDP port 53 está bloqueado)
    static const std::unordered_map<std::string, std::string> kGoogleAnycastMap = {
        {"oauth2.googleapis.com", "172.253.122.95"},
        {"www.googleapis.com", "142.250.185.202"},
        {"accounts.google.com", "172.217.16.202"},
        {"www.google.com", "142.250.185.202"}
    };

    auto fit = kGoogleAnycastMap.find(domain);
    if (fit != kGoogleAnycastMap.end())
    {
        MACHETE_LOG("[DNS]", "UDP blocked or timeout. Using Anycast static IP for '%s': %s",
                    domain.c_str(), fit->second.c_str());
        std::lock_guard<std::mutex> lock(mutex_);
        cache_[domain] = {fit->second, now + std::chrono::seconds(600)};
        return fit->second;
    }

    MACHETE_LOG("[DNS]", "Warning: Could not resolve '%s' across upstream DNS servers", domain.c_str());
    return "";
}

std::vector<std::string> DnsResolver::get_candidate_ips(const std::string &domain)
{
    std::vector<std::string> ips;
    std::string primary = resolve(domain);
    if (!primary.empty())
    {
        ips.push_back(primary);
    }

    static const std::unordered_map<std::string, std::vector<std::string>> kAnycastLists = {
        {"oauth2.googleapis.com", {"172.253.122.95", "142.250.185.202", "172.217.16.202", "216.58.214.202"}},
        {"www.googleapis.com", {"142.250.185.202", "172.217.16.202", "172.253.122.95", "216.58.214.202"}},
        {"accounts.google.com", {"172.217.16.202", "142.250.185.202", "172.253.122.95"}},
        {"www.google.com", {"142.250.185.202", "172.217.16.202"}}
    };

    auto it = kAnycastLists.find(domain);
    if (it != kAnycastLists.end())
    {
        for (const auto &fb_ip : it->second)
        {
            if (fb_ip != primary)
            {
                ips.push_back(fb_ip);
            }
        }
    }
    return ips;
}

} // namespace machete::cloud
