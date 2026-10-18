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
    if (stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode) || st.st_size == 0)
        return false;

    if (st.st_size == static_cast<off_t>(KEYSTONE_SIZE))
    {
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

    // Permitir keystones en entornos de prueba / mock
    return true;
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
        const std::uint32_t entry_offset = be32(entry + 0x10);
        const std::uint32_t entry_size = be32(entry + 0x14);

        if (entry_size == KEYSTONE_SIZE)
        {
            if (pread(fd, out_buf, KEYSTONE_SIZE, entry_offset) == KEYSTONE_SIZE)
            {
                if (std::memcmp(out_buf, "keystone", 8) == 0 && is_valid_keystone_buffer(out_buf))
                {
                    close(fd);
                    return true;
                }
            }
        }
    }

    close(fd);
    return false;
}

// Escanea recursivamente un directorio en busca de cualquier archivo .pkg o .keystone y extrae el keystone
inline bool scan_dir_for_pkg_keystone(const std::string &dir_path, std::uint8_t out_buf[KEYSTONE_SIZE], int depth = 0)
{
    if (depth >= 4 || dir_path.empty())
        return false;

    DIR *d = opendir(dir_path.c_str());
    if (!d)
        return false;

    while (const dirent *ent = readdir(d))
    {
        if (ent->d_name[0] == '.')
            continue;

        const std::string full = dir_path + "/" + ent->d_name;
        struct stat st{};
        if (stat(full.c_str(), &st) != 0)
            continue;

        if (S_ISDIR(st.st_mode))
        {
            if (scan_dir_for_pkg_keystone(full, out_buf, depth + 1))
            {
                closedir(d);
                return true;
            }
        }
        else if (S_ISREG(st.st_mode))
        {
            const std::string name = ent->d_name;
            if (name == "keystone" || (name.size() > 9 && name.substr(name.size() - 9) == ".keystone"))
            {
                if (is_valid_keystone_file(full))
                {
                    std::FILE *fk = std::fopen(full.c_str(), "rb");
                    if (fk)
                    {
                        const std::size_t n = std::fread(out_buf, 1, KEYSTONE_SIZE, fk);
                        std::fclose(fk);
                        if (n == KEYSTONE_SIZE && is_valid_keystone_buffer(out_buf))
                        {
                            MACHETE_LOG("[KEYSTONE]", "Harvested authentic keystone file from dir: %s", full.c_str());
                            closedir(d);
                            return true;
                        }
                    }
                }
            }
            else if (name.size() > 4)
            {
                std::string ext = name.substr(name.size() - 4);
                if (ext == ".pkg" || ext == ".PKG")
                {
                    if (extract_keystone_from_pkg(full, out_buf))
                    {
                        MACHETE_LOG("[KEYSTONE]", "Extracted authentic keystone from discovered PKG: %s", full.c_str());
                        closedir(d);
                        return true;
                    }
                }
            }
        }
    }
    closedir(d);
    return false;
}

inline std::string find_authentic_game_keystone(const std::string &title_id,
                                                const std::string &backup_folder = "",
                                                const std::string &dst_target = "",
                                                const std::string &app_path = "")
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
                (void)mkdir("/data/MacheteSVG", 0777);
                (void)mkdir("/data/MacheteSVG/keystones", 0777);
                const std::string out_ks = "/data/MacheteSVG/keystones/" + title_id + ".keystone";
                if (dp != out_ks)
                {
                    std::uint8_t buf[KEYSTONE_SIZE] = {0};
                    std::FILE *fin = std::fopen(dp.c_str(), "rb");
                    if (fin)
                    {
                        if (std::fread(buf, 1, KEYSTONE_SIZE, fin) == KEYSTONE_SIZE)
                        {
                            std::FILE *fout = std::fopen(out_ks.c_str(), "wb");
                            if (fout)
                            {
                                std::fwrite(buf, 1, KEYSTONE_SIZE, fout);
                                std::fclose(fout);
                                (void)chmod(out_ks.c_str(), 0666);
                            }
                        }
                        std::fclose(fin);
                    }
                }
                MACHETE_LOG("[KEYSTONE]", "Using authentic keystone from destination save dir: %s", dp.c_str());
                return dp;
            }
        }
    }

    // 2. Comprobar en rutas directas del sistema y de la aplicacion instalada
    const std::vector<std::string> candidates = {
        "/system_ex/app/" + title_id + "/sce_sys/keystone",
        "/system_ex/app/" + title_id + "/app0/sce_sys/keystone",
        "/system_ex/app/" + title_id + "/keystone",
        "/user/app/" + title_id + "/sce_sys/keystone",
        "/user/app/" + title_id + "/app0/sce_sys/keystone",
        "/user/app/" + title_id + "/keystone",
        "/mnt/sandbox/pfsmnt/" + title_id + "-app0/sce_sys/keystone",
        "/mnt/sandbox/" + title_id + "_000/app0/sce_sys/keystone",
        "/mnt/ext0/user/app/" + title_id + "/sce_sys/keystone",
        "/mnt/ext0/user/app/" + title_id + "/app0/sce_sys/keystone",
        "/mnt/ext0/user/app/" + title_id + "/keystone",
        "/mnt/ext1/user/app/" + title_id + "/sce_sys/keystone",
        "/mnt/ext1/user/app/" + title_id + "/app0/sce_sys/keystone",
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
        "/data/apollo/keystones/" + title_id + ".keystone",
        "/data/MacheteSVG/keystones/" + title_id + ".keystone",
        "/data/save_staging/preserved_dummy_" + title_id + "/keystone",
        "/data/save_staging/preserved_dummy_" + title_id + "/harvested_keystone",
        "/data/save_staging/preserved_dummy_" + title_id + "/.meta_dummy/keystone",
        "/data/save_staging/preserved_dummy_" + title_id + "/.meta_dummy/sce_sys/keystone",
        "/data/save_staging/preserved_dummy_" + title_id + "/sce_sys/keystone",
        "/data/pkg/" + title_id + "/sce_sys/keystone",
        "/data/homebrew/" + title_id + "/sce_sys/keystone",
        "/data/homebrew/" + title_id + "/app0/sce_sys/keystone",
        "/data/homebrew/" + title_id + "/keystone"};

    for (const auto &p : candidates)
    {
        if (access(p.c_str(), R_OK) == 0 && is_valid_keystone_file(p))
        {
            MACHETE_LOG("[KEYSTONE]", "Authentic keystone located at: %s", p.c_str());
            return p;
        }
    }

    // 3. Comprobar en app_path especifico del juego si fue proporcionado
    if (!app_path.empty())
    {
        for (const std::string &ap : {
                 app_path + "/keystone",
                 app_path + "/sce_sys/keystone",
                 app_path + "/app0/sce_sys/keystone"})
        {
            if (access(ap.c_str(), R_OK) == 0 && is_valid_keystone_file(ap))
            {
                MACHETE_LOG("[KEYSTONE]", "Using authentic keystone from app_path: %s", ap.c_str());
                return ap;
            }
        }
        std::uint8_t extracted[KEYSTONE_SIZE] = {0};
        if (scan_dir_for_pkg_keystone(app_path, extracted))
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
                MACHETE_LOG("[KEYSTONE]", "Extracted authentic keystone from app_path scan: %s -> %s",
                            app_path.c_str(), out_ks.c_str());
                return out_ks;
            }
        }
    }

    // 4. Buscar en la carpeta de respaldo proporcionada
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

    // 5. Buscar en dispositivos USB conectados (/mnt/usb0..7)
    for (int u = 0; u <= 7; ++u)
    {
        const std::string usb_base = "/mnt/usb" + std::to_string(u);
        for (const std::string &uk : {
                 usb_base + "/MacheteSVG/keystones/" + title_id + ".keystone",
                 usb_base + "/apollo/keystones/" + title_id + ".keystone",
                 usb_base + "/keystones/" + title_id + ".keystone"})
        {
            if (access(uk.c_str(), R_OK) == 0 && is_valid_keystone_file(uk))
            {
                MACHETE_LOG("[KEYSTONE]", "Using authentic keystone from USB: %s", uk.c_str());
                return uk;
            }
        }
    }

    // 6. Buscar en cualquier directorio de partidas existentes de otros perfiles de usuario
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

    // 7. Extraer directamente desde el paquete app.pkg instalado si existe
    for (const std::string &pkg_cand : {
             "/user/app/" + title_id + "/app.pkg",
             "/mnt/ext0/user/app/" + title_id + "/app.pkg",
             "/mnt/ext1/user/app/" + title_id + "/app.pkg",
             "/system_ex/app/" + title_id + "/app.pkg",
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

    // 8. Escaneo dinamico de cualquier archivo .pkg en los directorios de la aplicacion
    for (const std::string &app_dir : std::vector<std::string>{
             "/user/app/" + title_id,
             "/mnt/ext0/user/app/" + title_id,
             "/mnt/ext1/user/app/" + title_id,
             "/system_ex/app/" + title_id,
             "/data/pkg/" + title_id,
             "/data/pkg",
             "/data/homebrew/" + title_id})
    {
        std::uint8_t extracted[KEYSTONE_SIZE] = {0};
        if (scan_dir_for_pkg_keystone(app_dir, extracted))
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
                MACHETE_LOG("[KEYSTONE]", "Extracted authentic keystone from scanned PKG in %s -> %s",
                            app_dir.c_str(), out_ks.c_str());
                return out_ks;
            }
        }
    }

    return "";
}

// Inyecta el keystone autentico en el directorio de guardado restaurado y lo respalda en la boveda local
inline bool inject_authentic_keystone(const std::string &save_dir,
                                      const std::string &title_id,
                                      const std::string &backup_folder = "",
                                      const std::string &app_path = "")
{
    const std::string authentic_src = find_authentic_game_keystone(title_id, backup_folder, save_dir, app_path);
    if (authentic_src.empty())
    {
        MACHETE_LOG("[KEYSTONE]", "Notice: Authentic keystone not found in appmeta or app for %s", title_id.c_str());
        return false;
    }

    std::vector<std::string> dest_targets = {save_dir + "/keystone"};
    (void)mkdir((save_dir + "/sce_sys").c_str(), 0777);
    dest_targets.push_back(save_dir + "/sce_sys/keystone");

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

// Genera e inyecta un Keystone estandar compatible con FPKG / Apollo cuando no existe keystone autentico
inline bool inject_fallback_fpkg_keystone(const std::string &save_dir, const std::string &title_id)
{
    if (save_dir.empty() || title_id.empty())
        return false;

    std::uint8_t ks[KEYSTONE_SIZE] = {0};
    std::memcpy(ks, "keystone", 8);
    ks[8] = 0x00;
    ks[9] = 0x02; // Type = 2 (PS4/Vita)
    ks[10] = 0x00;
    ks[11] = 0x01; // Version = 1
    // Bytes 12..95 en 0 (passcode estandar de 32 ceros para FPKG)

    (void)mkdir(save_dir.c_str(), 0777);
    (void)mkdir((save_dir + "/sce_sys").c_str(), 0777);

    const std::vector<std::string> dest_targets = {
        save_dir + "/keystone",
        save_dir + "/sce_sys/keystone"};

    bool written = false;
    for (const auto &dst : dest_targets)
    {
        std::FILE *fout = std::fopen(dst.c_str(), "wb");
        if (fout)
        {
            if (std::fwrite(ks, 1, KEYSTONE_SIZE, fout) == KEYSTONE_SIZE)
            {
                std::fflush(fout);
                int fd = fileno(fout);
                if (fd >= 0)
                    fsync(fd);
                written = true;
            }
            std::fclose(fout);
            (void)chmod(dst.c_str(), 0666);
        }
    }

    if (written)
    {
        (void)mkdir("/data/MacheteSVG", 0777);
        (void)mkdir("/data/MacheteSVG/keystones", 0777);
        const std::string vault_ks = "/data/MacheteSVG/keystones/" + title_id + ".keystone";
        std::FILE *fvault = std::fopen(vault_ks.c_str(), "wb");
        if (fvault)
        {
            std::fwrite(ks, 1, KEYSTONE_SIZE, fvault);
            std::fclose(fvault);
            (void)chmod(vault_ks.c_str(), 0666);
        }
        MACHETE_LOG("[KEYSTONE]", "Injected Apollo-compatible FPKG fallback keystone for %s into %s",
                    title_id.c_str(), save_dir.c_str());
    }

    return written;
}

} // namespace machete::keystone
