// MacheteSVG - Client Interface for Machete PFS Helper Daemon
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/embedded_pfs_helper.hpp"
#include "core/logger.hpp"

#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#if defined(__PROSPERO__) || defined(__Prospero__) || defined(__PS5__)
struct HelperSockaddrIn
{
    std::uint8_t length;
    std::uint8_t family;
    std::uint16_t port;
    std::uint32_t address;
    std::uint16_t virtual_port;
    std::uint8_t zero[6];
};

extern "C"
{
    int sceNetInit(void);
    int sceNetConnect(int socket, const void *address, std::uint32_t address_length);
    int sceNetSend(int socket, const void *data, std::size_t length, int flags);
    int sceNetRecv(int socket, void *data, std::size_t length, int flags);
    int sceNetSetsockopt(int socket, int level, int option, const void *value, std::uint32_t size);
    int sceNetSocket(const char *name, int domain, int type, int protocol);
    int sceNetSocketClose(int socket);
}
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

namespace machete::pfs::helper
{

constexpr int kHelperPort = 9033;
constexpr const char *kCmdPath = "/download0/machete_pfs_cmd";
constexpr const char *kResPath = "/download0/machete_pfs_res";
constexpr const char *kCmdDataPath = "/data/machete_pfs_cmd";
constexpr const char *kResDataPath = "/data/machete_pfs_res";
constexpr const char *kReadyPath = "/download0/machete_pfs_ready";
constexpr const char *kReadyDataPath = "/data/machete_pfs_ready";

inline bool send_command_via_socket(const std::string &cmd, std::string &out_res)
{
#if defined(__PROSPERO__) || defined(__Prospero__) || defined(__PS5__)
    const int sock = sceNetSocket("machete_pfs_cli", 2, 1, 6);
    if (sock < 0)
        return false;

    constexpr int sock_level = 0xffff;
    constexpr int timeout_us = 3'000'000;
    sceNetSetsockopt(sock, sock_level, 0x1105, &timeout_us, sizeof(timeout_us));
    sceNetSetsockopt(sock, sock_level, 0x1106, &timeout_us, sizeof(timeout_us));
    sceNetSetsockopt(sock, sock_level, 0x1109, &timeout_us, sizeof(timeout_us));

    const HelperSockaddrIn endpoint{sizeof(HelperSockaddrIn),
                                    2,
                                    static_cast<std::uint16_t>((kHelperPort << 8) | (kHelperPort >> 8)),
                                    0x0100007f, // 127.0.0.1
                                    0,
                                    {0}};

    if (sceNetConnect(sock, &endpoint, sizeof(endpoint)) < 0)
    {
        sceNetSocketClose(sock);
        return false;
    }

    std::string full_cmd = cmd + "\n";
    if (sceNetSend(sock, full_cmd.c_str(), full_cmd.size(), 0) <= 0)
    {
        sceNetSocketClose(sock);
        return false;
    }

    char buf[1024] = {0};
    int r = sceNetRecv(sock, buf, sizeof(buf) - 1, 0);
    sceNetSocketClose(sock);

    if (r > 0)
    {
        buf[r] = '\0';
        out_res = buf;
        return true;
    }
    return false;
#else
    (void)cmd;
    (void)out_res;
    return false;
#endif
}

inline bool send_command_via_file_ipc(const std::string &cmd, std::string &out_res)
{
    const char *cmd_target = kCmdPath;
    const char *res_target = kResPath;

    if (access("/download0", W_OK) != 0)
    {
        cmd_target = kCmdDataPath;
        res_target = kResDataPath;
    }

    (void)unlink(res_target);

    std::FILE *fc = std::fopen(cmd_target, "w");
    if (!fc)
    {
        cmd_target = kCmdDataPath;
        res_target = kResDataPath;
        fc = std::fopen(cmd_target, "w");
    }

    if (!fc)
        return false;

    std::fputs(cmd.c_str(), fc);
    std::fputc('\n', fc);
    std::fclose(fc);
    (void)chmod(cmd_target, 0666);

    for (int i = 0; i < 60; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (access(res_target, R_OK) == 0)
        {
            std::FILE *fr = std::fopen(res_target, "r");
            if (fr)
            {
                char buf[1024] = {0};
                if (std::fgets(buf, sizeof(buf), fr))
                {
                    out_res = buf;
                }
                std::fclose(fr);
                (void)unlink(res_target);
                return !out_res.empty();
            }
        }
    }
    return false;
}

inline bool execute_helper_cmd(const std::string &cmd, std::string &out_res)
{
    if (send_command_via_socket(cmd, out_res))
        return true;
    return send_command_via_file_ipc(cmd, out_res);
}

inline bool is_helper_alive()
{
    std::string res;
    if (send_command_via_socket("PING", res))
    {
        if (res.find("PONG") != std::string::npos)
            return true;
    }
    if (send_command_via_file_ipc("PING", res))
    {
        if (res.find("PONG") != std::string::npos)
            return true;
    }
    return false;
}

inline bool launch_helper_via_elf_loader()
{
#if defined(__PROSPERO__) || defined(__Prospero__) || defined(__PS5__)
    static bool s_net_init = false;
    if (!s_net_init)
    {
        (void)sceNetInit();
        s_net_init = true;
    }

    const char *daemon_paths[] = {
        "/app0/daemon/machete_pfs_helper.elf",
        "/system_ex/app/PPSA99700/daemon/machete_pfs_helper.elf",
        "/data/homebrew/PPSA99700/daemon/machete_pfs_helper.elf",
        "daemon/machete_pfs_helper.elf",
        "/data/machete_pfs_helper.elf"
    };

    const void *payload_data = nullptr;
    std::size_t payload_size = 0;
    std::vector<std::uint8_t> disk_buffer;

    for (const char *path : daemon_paths)
    {
        const int fd = open(path, O_RDONLY);
        if (fd >= 0)
        {
            struct stat st{};
            if (fstat(fd, &st) == 0 && st.st_size > 1024)
            {
                disk_buffer.resize(static_cast<std::size_t>(st.st_size));
                auto bytes = read(fd, disk_buffer.data(), disk_buffer.size());
                if (bytes == st.st_size)
                {
                    payload_data = disk_buffer.data();
                    payload_size = disk_buffer.size();
                    close(fd);
                    break;
                }
            }
            close(fd);
        }
    }

    if (!payload_data || payload_size == 0)
    {
        payload_data = kEmbeddedPfsHelperElf;
        payload_size = kEmbeddedPfsHelperElfSize;
    }

    if (!payload_data || payload_size == 0)
    {
        MACHETE_LOG("[PFS_HELPER]", "No helper payload binary found to inject");
        return false;
    }

    for (const std::uint16_t port : {static_cast<std::uint16_t>(9021), static_cast<std::uint16_t>(9020), static_cast<std::uint16_t>(9027)})
    {
        MACHETE_LOG("[PFS_HELPER]", "Attempting to launch PFS helper via ELF loader on 127.0.0.1:%u", port);
        const int sock = sceNetSocket("machete_pfs_ldr", 2, 1, 6);
        if (sock < 0)
            continue;

        constexpr int sock_level = 0xffff;
        constexpr int timeout_us = 2'000'000;
        sceNetSetsockopt(sock, sock_level, 0x1105, &timeout_us, sizeof(timeout_us));
        sceNetSetsockopt(sock, sock_level, 0x1106, &timeout_us, sizeof(timeout_us));
        sceNetSetsockopt(sock, sock_level, 0x1109, &timeout_us, sizeof(timeout_us));

        const HelperSockaddrIn endpoint{sizeof(HelperSockaddrIn),
                                        2,
                                        static_cast<std::uint16_t>((port << 8) | (port >> 8)),
                                        0x0100007f, // 127.0.0.1
                                        0,
                                        {0}};

        if (sceNetConnect(sock, &endpoint, sizeof(endpoint)) < 0)
        {
            sceNetSocketClose(sock);
            continue;
        }

        MACHETE_LOG("[PFS_HELPER]", "Connected to ELF loader at port %u, streaming helper (%zu bytes)...",
                    port, payload_size);

        const auto *bytes = static_cast<const char *>(payload_data);
        std::size_t remaining = payload_size;
        bool stream_ok = true;

        while (remaining > 0)
        {
            const std::size_t chunk = std::min<std::size_t>(remaining, 16384);
            const int sent = sceNetSend(sock, bytes, chunk, 0);
            if (sent <= 0)
            {
                stream_ok = false;
                break;
            }
            bytes += sent;
            remaining -= static_cast<std::size_t>(sent);
        }

        sceNetSocketClose(sock);

        if (stream_ok)
        {
            MACHETE_LOG("[PFS_HELPER]", "PFS Helper payload successfully transmitted to port %u", port);
            return true;
        }
    }
    return false;
#else
    return true;
#endif
}

inline bool ensure_helper_ready()
{
    if (is_helper_alive())
    {
        return true;
    }

    MACHETE_LOG("[PFS_HELPER]", "Helper not alive, launching via ELF loader...");
    (void)unlink(kReadyPath);
    (void)unlink(kReadyDataPath);
    (void)unlink(kCmdPath);
    (void)unlink(kCmdDataPath);
    (void)unlink(kResPath);
    (void)unlink(kResDataPath);

    if (!launch_helper_via_elf_loader())
    {
        MACHETE_LOG("[PFS_HELPER]", "Failed to transmit helper payload to ELF loader");
        return false;
    }

    // Esperar hasta 2.5 segundos a que el helper responda a PING
    for (int i = 0; i < 50; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (is_helper_alive())
        {
            MACHETE_LOG("[PFS_HELPER]", "Machete PFS Helper confirmed ALIVE and responsive on attempt %d", i + 1);
            return true;
        }
    }

    MACHETE_LOG("[PFS_HELPER]", "PFS Helper alive poll timed out");
    return false;
}

inline bool mount_container(const std::string &image_path, const std::string &mount_point, const std::uint8_t *plain_key = nullptr)
{
    if (!ensure_helper_ready())
    {
        MACHETE_LOG("[PFS_HELPER]", "Cannot mount %s: helper daemon is not available", image_path.c_str());
        return false;
    }

    std::string cmd = "MOUNT \"" + image_path + "\" \"" + mount_point + "\"";
    if (plain_key)
    {
        char hex[65] = {0};
        for (int i = 0; i < 32; ++i)
        {
            std::snprintf(hex + i * 2, 3, "%02x", plain_key[i]);
        }
        cmd += " ";
        cmd += hex;
    }

    std::string res;
    if (execute_helper_cmd(cmd, res))
    {
        MACHETE_LOG("[PFS_HELPER]", "Helper mount response: %s", res.c_str());
        if (res.find("\"status\": \"mounted\"") != std::string::npos || res.find("\"rc\": 0") != std::string::npos)
        {
            return true;
        }
        // Algunos contenedores montados devuelven descriptor positivo
        if (res.find("\"rc\":") != std::string::npos && res.find("\"rc\": -") == std::string::npos)
        {
            return true;
        }
    }
    return false;
}

inline bool unmount_container(const std::string &mount_point)
{
    std::string cmd = "UMOUNT \"" + mount_point + "\"";
    std::string res;
    if (execute_helper_cmd(cmd, res))
    {
        MACHETE_LOG("[PFS_HELPER]", "Helper umount response: %s", res.c_str());
        return (res.find("\"status\": \"unmounted\"") != std::string::npos || res.find("\"rc\": 0") != std::string::npos);
    }
    return false;
}

} // namespace machete::pfs::helper
