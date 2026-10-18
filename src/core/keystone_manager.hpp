// MacheteSVG - Keystone Verification & Authentic Injection
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/logger.hpp"

#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace machete::keystone
{

constexpr std::size_t KEYSTONE_SIZE = 96; // 0x60 bytes

inline bool is_valid_keystone_buffer(const std::uint8_t buf[KEYSTONE_SIZE])
{
    if (!buf)
        return false;
    for (std::size_t i = 0; i < KEYSTONE_SIZE; ++i)
    {
        if (buf[i] != 0)
            return true;
    }
    return false;
}

inline bool is_valid_keystone_file(const std::string &path)
{
    struct stat st{};
    if (stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode) || st.st_size != static_cast<off_t>(KEYSTONE_SIZE))
        return false;

    std::FILE *f = std::fopen(path.c_str(), "rb");
    if (!f)
        return false;

    std::uint8_t buf[KEYSTONE_SIZE] = {0};
    const std::size_t n = std::fread(buf, 1, KEYSTONE_SIZE, f);
    std::fclose(f);
    if (n != KEYSTONE_SIZE)
        return false;

    return is_valid_keystone_buffer(buf);
}

// Extrae el keystone de 96 bytes autentico desde la tabla de entradas de un archivo PKG
inline bool extract_keystone_from_pkg(const std::string &pkg_path, std::uint8_t out_buf[KEYSTONE_SIZE])
{
    int fd = open(pkg_path.c_str(), O_RDONLY);
    if (fd < 0)
        return false;

    std::uint8_t hdr[0x100] = {0};
    if (pread(fd, hdr, sizeof(hdr), 0) != sizeof(hdr))
    {
        close(fd);
        return false;
    }

    if (hdr[0] != 0x7F || hdr[1] != 'C' || hdr[2] != 'N' || hdr[3] != 'T')
    {
        close(fd);
        return false;
    }

    auto be32 = [](const std::uint8_t *p) -> std::uint32_t {
        return (static_cast<std::uint32_t>(p[0]) << 24) |
               (static_cast<std::uint32_t>(p[1]) << 16) |
               (static_cast<std::uint32_t>(p[2]) << 8) |
               static_cast<std::uint32_t>(p[3]);
    };

    const std::uint32_t entry_count = be32(hdr + 0x10);
    const std::uint32_t table_offset = be32(hdr + 0x18);

    if (entry_count == 0 || entry_count > 4096 || table_offset < 0x20 || table_offset > 0x1000000)
    {
        close(fd);
        return false;
    }

    std::vector<std::uint8_t> table(entry_count * 32);
    if (pread(fd, table.data(), table.size(), table_offset) != static_cast<ssize_t>(table.size()))
    {
        close(fd);
        return false;
    }

    for (std::size_t i = 0; i < entry_count; ++i)
    {
        const std::uint8_t *entry = table.data() + i * 32;
        const std::uint32_t entry_id = be32(entry + 0x00);
        const std::uint32_t entry_offset = be32(entry + 0x10);
        const std::uint32_t entry_size = be32(entry + 0x14);

        if (entry_id == 0x1200 && entry_size == KEYSTONE_SIZE)
        {
            if (pread(fd, out_buf, KEYSTONE_SIZE, entry_offset) == KEYSTONE_SIZE)
            {
                close(fd);
                return is_valid_keystone_buffer(out_buf);
            }
        }
    }

    close(fd);
    return false;
}

inline std::string find_authentic_game_keystone(const std::string &title_id,
                                                const std::string &backup_folder = "",
                                                const std::string &dst_target = "")
{
    // 1. Priorizar el keystone del destino de guardado si existe (por ejemplo el generado por un Save Dummy)
    if (!dst_target.empty())
    {
        for (const std::string &dp : {
                 dst_target + "/keystone",
                 dst_target + "/sce_sys/keystone"})
        {
            if (access(dp.c_str(), R_OK) == 0 && is_valid_keystone_file(dp))
            {
                MACHETE_LOG("[KEYSTONE]", "Using authentic keystone from destination save dir: %s", dp.c_str());
                return dp;
            }
        }
    }

    // 2. Comprobar en rutas directas del sistema y de la aplicacion
    const std::vector<std::string> candidates = {
        "/system_ex/app/" + title_id + "/sce_sys/keystone",
        "/system_ex/app/" + title_id + "/keystone",
        "/user/app/" + title_id + "/sce_sys/keystone",
        "/user/app/" + title_id + "/keystone",
        "/mnt/sandbox/pfsmnt/" + title_id + "-app0/sce_sys/keystone",
        "/mnt/sandbox/" + title_id + "_000/app0/sce_sys/keystone",
        "/mnt/ext0/user/app/" + title_id + "/sce_sys/keystone",
        "/mnt/ext0/user/app/" + title_id + "/keystone",
        "/mnt/ext1/user/app/" + title_id + "/sce_sys/keystone",
        "/mnt/ext1/user/app/" + title_id + "/keystone",
        "/system_data/priv/appmeta/" + title_id + "/sce_sys/keystone",
        "/system_data/priv/appmeta/" + title_id + "/keystone",
        "/system_data/appmeta/" + title_id + "/keystone",
        "/user/appmeta/" + title_id + "/sce_sys/keystone",
        "/user/appmeta/" + title_id + "/keystone",
        "/user/appmeta/external/" + title_id + "/sce_sys/keystone",
        "/user/appmeta/external/" + title_id + "/keystone",
        "/mnt/ext0/user/appmeta/" + title_id + "/sce_sys/keystone",
        "/mnt/ext1/user/appmeta/" + title_id + "/sce_sys/keystone",
        "/data/apollo/" + title_id + ".keystone",
        "/data/MacheteSVG/keystones/" + title_id + ".keystone",
        "/data/save_staging/preserved_dummy_" + title_id + "/keystone",
        "/data/save_staging/preserved_dummy_" + title_id + "/harvested_keystone",
        "/data/save_staging/preserved_dummy_" + title_id + "/.meta_dummy/keystone",
        "/data/save_staging/preserved_dummy_" + title_id + "/.meta_dummy/sce_sys/keystone",
        "/data/save_staging/preserved_dummy_" + title_id + "/sce_sys/keystone",
        "/data/pkg/" + title_id + "/sce_sys/keystone",
        "/data/homebrew/" + title_id + "/sce_sys/keystone",
        "/data/homebrew/" + title_id + "/keystone"};

    for (const auto &p : candidates)
    {
        if (access(p.c_str(), R_OK) == 0 && is_valid_keystone_file(p))
        {
            MACHETE_LOG("[KEYSTONE]", "Authentic keystone located at: %s", p.c_str());
            return p;
        }
    }

    // 3. Buscar en la carpeta de respaldo proporcionada
    if (!backup_folder.empty())
    {
        for (const std::string &bp : {
                 backup_folder + "/keystone",
                 backup_folder + "/sce_sys/keystone",
                 backup_folder + "/.meta/keystone",
                 backup_folder + "/.meta/sce_sys/keystone",
                 backup_folder + "/meta/keystone",
                 backup_folder + "/meta/sce_sys/keystone",
                 backup_folder + "/.meta_dummy/keystone",
                 backup_folder + "/.meta_dummy/sce_sys/keystone"})
        {
            if (access(bp.c_str(), R_OK) == 0 && is_valid_keystone_file(bp))
            {
                MACHETE_LOG("[KEYSTONE]", "Using preserved keystone from backup: %s", bp.c_str());
                return bp;
            }
        }
    }

    // 4. Buscar en cualquier directorio de partidas existentes de otros perfiles de usuario
    for (const char *hroot : {"/user/home", "/mnt/ext0/user/home", "/mnt/ext1/user/home"})
    {
        DIR *hd = opendir(hroot);
        if (hd)
        {
            while (const dirent *ue = readdir(hd))
            {
                if (ue->d_name[0] == '.')
                    continue;
                const std::string ks_path = std::string(hroot) + "/" + ue->d_name + "/savedata/" + title_id + "/keystone";
                if (access(ks_path.c_str(), R_OK) == 0 && is_valid_keystone_file(ks_path))
                {
                    closedir(hd);
                    MACHETE_LOG("[KEYSTONE]", "Using authentic keystone from existing user savedata: %s", ks_path.c_str());
                    return ks_path;
                }
            }
            closedir(hd);
        }
    }

    // 5. Extraer directamente desde el paquete app.pkg instalado si existe
    for (const std::string &pkg_cand : {
             "/user/app/" + title_id + "/app.pkg",
             "/mnt/ext0/user/app/" + title_id + "/app.pkg",
             "/mnt/ext1/user/app/" + title_id + "/app.pkg",
             "/data/pkg/" + title_id + ".pkg",
             "/data/pkg/" + title_id + "/app.pkg",
             "/data/homebrew/" + title_id + "/app.pkg"})
    {
        std::uint8_t extracted[KEYSTONE_SIZE] = {0};
        if (extract_keystone_from_pkg(pkg_cand, extracted))
        {
            (void)mkdir("/data/MacheteSVG", 0777);
            (void)mkdir("/data/MacheteSVG/keystones", 0777);
            const std::string out_ks = "/data/MacheteSVG/keystones/" + title_id + ".keystone";
            std::FILE *fout = std::fopen(out_ks.c_str(), "wb");
            if (fout)
            {
                std::fwrite(extracted, 1, KEYSTONE_SIZE, fout);
                std::fclose(fout);
                (void)chmod(out_ks.c_str(), 0666);
                MACHETE_LOG("[KEYSTONE]", "Extracted authentic keystone from PKG %s -> %s",
                            pkg_cand.c_str(), out_ks.c_str());
                return out_ks;
            }
        }
    }

    return "";
}

// Inyecta el keystone autentico en el directorio de guardado restaurado y lo respalda en la boveda local
inline bool inject_authentic_keystone(const std::string &save_dir, const std::string &title_id, const std::string &backup_folder = "")
{
    const std::string authentic_src = find_authentic_game_keystone(title_id, backup_folder, save_dir);
    if (authentic_src.empty())
    {
        MACHETE_LOG("[KEYSTONE]", "Notice: Authentic keystone not found in appmeta or app for %s", title_id.c_str());
        return false;
    }

    std::vector<std::string> dest_targets = {save_dir + "/keystone"};
    if (access((save_dir + "/sce_sys").c_str(), F_OK) == 0)
    {
        dest_targets.push_back(save_dir + "/sce_sys/keystone");
    }

    std::FILE *fin = std::fopen(authentic_src.c_str(), "rb");
    if (!fin)
        return false;

    std::uint8_t buffer[KEYSTONE_SIZE];
    const std::size_t n = std::fread(buffer, 1, KEYSTONE_SIZE, fin);
    std::fclose(fin);

    if (n != KEYSTONE_SIZE)
        return false;

    for (const auto &dest_keystone : dest_targets)
    {
        std::FILE *fout = std::fopen(dest_keystone.c_str(), "wb");
        if (fout)
        {
            std::fwrite(buffer, 1, KEYSTONE_SIZE, fout);
            std::fflush(fout);
            int fd = fileno(fout);
            if (fd >= 0)
            {
                fsync(fd);
            }
            std::fclose(fout);
            (void)chmod(dest_keystone.c_str(), 0666);
        }
    }

    // Guardar copia permanente en la bóveda de keystones
    (void)mkdir("/data/MacheteSVG", 0777);
    (void)mkdir("/data/MacheteSVG/keystones", 0777);
    const std::string cache_keystone = "/data/MacheteSVG/keystones/" + title_id + ".keystone";
    std::FILE *fcache = std::fopen(cache_keystone.c_str(), "wb");
    if (fcache)
    {
        std::fwrite(buffer, 1, KEYSTONE_SIZE, fcache);
        std::fclose(fcache);
        (void)chmod(cache_keystone.c_str(), 0666);
    }

    MACHETE_LOG("[KEYSTONE]", "Authentic keystone injected successfully into %s (from %s)",
                save_dir.c_str(), authentic_src.c_str());
    return true;
}

} // namespace machete::keystone
