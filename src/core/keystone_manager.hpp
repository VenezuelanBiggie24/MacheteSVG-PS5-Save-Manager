// MacheteSVG - Keystone Verification & Authentic Injection
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/logger.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace machete::keystone
{

constexpr std::size_t KEYSTONE_SIZE = 96; // 0x60 bytes

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

    bool any_nonzero = false;
    for (std::size_t i = 0; i < KEYSTONE_SIZE; ++i)
    {
        if (buf[i] != 0)
        {
            any_nonzero = true;
            break;
        }
    }
    return any_nonzero;
}

inline std::string find_authentic_game_keystone(const std::string &title_id, const std::string &backup_folder = "")
{
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
        "/data/save_staging/preserved_dummy_" + title_id + "/.meta_dummy/keystone",
        "/data/save_staging/preserved_dummy_" + title_id + "/.meta_dummy/sce_sys/keystone",
        "/data/save_staging/preserved_dummy_" + title_id + "/harvested_keystone",
        "/data/save_staging/preserved_dummy_" + title_id + "/keystone",
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

    return "";
}

// Inyecta el keystone autentico en el directorio de guardado restaurado y lo respalda en la boveda local
inline bool inject_authentic_keystone(const std::string &save_dir, const std::string &title_id, const std::string &backup_folder = "")
{
    const std::string authentic_src = find_authentic_game_keystone(title_id, backup_folder);
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
