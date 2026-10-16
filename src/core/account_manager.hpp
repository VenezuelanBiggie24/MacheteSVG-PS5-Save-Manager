// MacheteSVG - Account ID Discovery & Alignment Manager
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/logger.hpp"
#include "core/sfo_resigner.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace machete::account
{

// Lee un Account ID de 64 bits de un archivo binario si no es 0
inline std::uint64_t read_account_id_from_file(const std::string &path, off_t offset)
{
    std::FILE *f = std::fopen(path.c_str(), "rb");
    if (!f)
        return 0;

    if (fseek(f, offset, SEEK_SET) != 0)
    {
        std::fclose(f);
        return 0;
    }

    std::uint64_t val = 0;
    if (std::fread(&val, 1, sizeof(val), f) != sizeof(val))
    {
        std::fclose(f);
        return 0;
    }
    std::fclose(f);
    return val;
}

// Busca el Account ID legítimo activo en la consola para el usuario
inline std::uint64_t detect_active_account_id(std::uint32_t user_id)
{
    char uid_hex[32];
    std::snprintf(uid_hex, sizeof(uid_hex), "%x", user_id);

    // 1. Probar en las rutas del sistema (/system_data/priv/home/<uid>/)
    const std::vector<std::pair<std::string, off_t>> sys_paths = {
        {"/system_data/priv/home/" + std::string(uid_hex) + "/auth.dat", 0x10},
        {"/system_data/priv/home/" + std::string(uid_hex) + "/auth.dat", 0x08},
        {"/system_data/priv/home/" + std::string(uid_hex) + "/account.dat", 0x00},
        {"/system_data/priv/home/" + std::string(uid_hex) + "/user.dat", 0x20}};

    for (const auto &sp : sys_paths)
    {
        std::uint64_t acc = read_account_id_from_file(sp.first, sp.second);
        if (acc != 0 && acc != ~0ULL)
        {
            MACHETE_LOG("[ACCOUNT]", "Account ID detected in %s (offset 0x%lx): 0x%04llx...%04llx",
                        sp.first.c_str(), static_cast<unsigned long>(sp.second),
                        static_cast<unsigned long long>((acc >> 48) & 0xffff),
                        static_cast<unsigned long long>(acc & 0xffff));
            return acc;
        }
    }

    // 2. Probar en metadatos y partidas existentes de PS4 (savedata_meta/user/* y savedata/*)
    for (const std::string &base_sfo_dir : {
             "/user/home/" + std::string(uid_hex) + "/savedata_meta/user",
             "/user/home/" + std::string(uid_hex) + "/savedata"})
    {
        DIR *dir = opendir(base_sfo_dir.c_str());
        if (!dir)
            continue;

        while (const dirent *ent = readdir(dir))
        {
            if (ent->d_name[0] == '.')
                continue;
            const std::string game_dir = base_sfo_dir + "/" + ent->d_name;
            for (const std::string &sfo_cand : {
                     game_dir + "/param.sfo",
                     game_dir + "/sce_sys/param.sfo"})
            {
                std::uint64_t acc = 0;
                if (sfo::read_account_id_from_sfo(sfo_cand, acc) && acc != 0 && acc != ~0ULL)
                {
                    MACHETE_LOG("[ACCOUNT]", "Account ID recovered from existing SFO (%s): 0x%04llx...%04llx",
                                sfo_cand.c_str(),
                                static_cast<unsigned long long>((acc >> 48) & 0xffff),
                                static_cast<unsigned long long>(acc & 0xffff));
                    closedir(dir);
                    return acc;
                }
            }
        }
        closedir(dir);
    }


    // 3. Probar en las bases de datos del sistema si estan accesibles
    for (const std::string &db_path : {
             "/system_data/priv/mms/system.db",
             "/system_data/priv/mms/user.db"})
    {
        if (access(db_path.c_str(), R_OK) == 0)
        {
            std::FILE *fdb = std::fopen(db_path.c_str(), "rb");
            if (fdb)
            {
                std::vector<std::uint8_t> buf(65536);
                std::size_t n = std::fread(buf.data(), 1, buf.size(), fdb);
                std::fclose(fdb);
                for (std::size_t i = 0; i + 12 <= n; ++i)
                {
                    std::uint32_t candidate_uid = 0;
                    std::memcpy(&candidate_uid, buf.data() + i, 4);
                    if (candidate_uid == user_id)
                    {
                        std::uint64_t acc = 0;
                        std::memcpy(&acc, buf.data() + i + 4, 8);
                        if (acc != 0 && acc != ~0ULL && acc > 0x000000000000FFFFULL)
                        {
                            MACHETE_LOG("[ACCOUNT]", "Account ID recovered from %s: 0x%04llx...%04llx",
                                        db_path.c_str(),
                                        static_cast<unsigned long long>((acc >> 48) & 0xffff),
                                        static_cast<unsigned long long>(acc & 0xffff));
                            return acc;
                        }
                    }
                }
            }
        }
    }

    // 4. Si no hay otra referencia, fallback seguro basado en el estándar offline de Apollo/PS5-Activator
    std::uint64_t fallback_acc = 0x0100000000000000ULL | (static_cast<std::uint64_t>(user_id) & 0xFFFFFFFFULL);
    if (fallback_acc == 0)
        fallback_acc = 0x0100000000000001ULL;

    MACHETE_LOG("[ACCOUNT]", "Using offline fallback Account ID for UserID 0x%x: 0x%04llx...%04llx",
                user_id,
                static_cast<unsigned long long>((fallback_acc >> 48) & 0xffff),
                static_cast<unsigned long long>(fallback_acc & 0xffff));
    return fallback_acc;
}

} // namespace machete::account
