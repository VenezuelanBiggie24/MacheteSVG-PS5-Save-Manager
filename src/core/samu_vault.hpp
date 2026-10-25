// MacheteSVG - Hardware Cryptographic Vault & SAMU Key Archiver
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/logger.hpp"
#include "core/sha256_hmac.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace machete::vault
{

constexpr std::uint32_t VAULT_MAGIC = 0x544C564D; // "MVLT" (Machete Vault)
constexpr std::uint32_t VAULT_VERSION = 1;
constexpr unsigned long PFS_DECRYPT_KEK_CMD = 0xc0845302; // ioctl para /dev/pfsmgr

#pragma pack(push, 1)
struct CryptoVaultRecord
{
    std::uint32_t magic = VAULT_MAGIC;
    std::uint32_t version = VAULT_VERSION;
    std::uint32_t local_user_id = 0;       // ej. 0x105f615a
    std::uint32_t flags = 0;               // 1 = tiene decrypted_pfs_key
    char title_id[16] = {0};               // ej. "PPSA13427"
    char platform[8] = {0};               // "PS5" o "PS4"
    std::uint8_t open_psid[16] = {0};      // OpenPSID de la consola fisica
    std::uint8_t sealed_blob[96] = {0};    // 96-byte encrypted SAMU key blob
    std::uint8_t decrypted_pfs_key[32] = {0}; // 32-byte plaintext AES key si SAMU lo descifro
    std::uint8_t integrity_sha256[32] = {0};
};
#pragma pack(pop)

// Lee 96 bytes en el offset especificado
inline bool read_sealed_blob(const std::string &path, off_t offset, std::uint8_t out_blob[96])
{
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0)
        return false;
    ssize_t n = pread(fd, out_blob, 96, offset);
    close(fd);
    return (n == 96);
}

// Localiza y lee el blob sellado de 96 bytes dentro de un contenedor PS5 (sdimg_*)
// En PS5 nativo, el sealed key reside exactamente en el offset 0x800 (2048) del sdimg (ps5upload / garlic-savemgr)
inline bool read_ps5_sealed_blob(const std::string &sdimg_path, std::uint8_t out_blob[96], off_t *out_offset = nullptr)
{
    if (sdimg_path.empty() || !out_blob)
        return false;

    int fd = open(sdimg_path.c_str(), O_RDONLY);
    if (fd < 0)
        return false;

    // 1. Comprobar primero el offset estandar de PS5 nativo 0x800 (2048)
    std::uint8_t blob_800[96] = {0};
    const ssize_t r_800 = pread(fd, blob_800, 96, 0x800);
    if (r_800 == 96)
    {
        bool non_zero = false;
        for (int i = 0; i < 96; ++i)
        {
            if (blob_800[i] != 0)
            {
                non_zero = true;
                break;
            }
        }
        if (non_zero)
        {
            close(fd);
            std::memcpy(out_blob, blob_800, 96);
            if (out_offset)
                *out_offset = 0x800;
            return true;
        }
    }

    // 2. Fallback: escanear los primeros 64 KB buscando la firma "pfsSKKey"
    constexpr std::size_t kScanBytes = 64 * 1024;
    std::vector<std::uint8_t> buf(kScanBytes, 0);
    const ssize_t n = pread(fd, buf.data(), kScanBytes, 0);
    close(fd);

    if (n < 96)
        return false;

    static const std::uint8_t kPfsSkKeyMagic[8] = {'p', 'f', 's', 'S', 'K', 'K', 'e', 'y'};
    const std::size_t limit = static_cast<std::size_t>(n) - 96;
    for (std::size_t off = 0; off <= limit; ++off)
    {
        if (std::memcmp(buf.data() + off, kPfsSkKeyMagic, 8) == 0)
        {
            std::memcpy(out_blob, buf.data() + off, 96);
            if (out_offset)
                *out_offset = static_cast<off_t>(off);
            return true;
        }
    }

    return false;
}

// Descifra el blob sellado a traves del driver de kernel /dev/pfsmgr utilizando SAMU
inline bool decrypt_blob_via_pfsmgr(const std::uint8_t in_blob[96], std::uint8_t out_plain_key[32],
                                    bool allow_zero_key_for_ps5 = false)
{
    if (!in_blob || !out_plain_key)
        return false;

    // Solo invocar /dev/pfsmgr cuando el proceso ya cuenta con privilegios root (lapy_jb_daemon)
    if (getuid() != 0 || access("/dev/pfsmgr", F_OK) != 0)
        return false;

    int pfd = open("/dev/pfsmgr", O_RDWR);
    if (pfd < 0)
    {
        MACHETE_LOG("[VAULT]", "Could not open /dev/pfsmgr (errno=%d)", errno);
        return false;
    }

    // Buffer de 512 bytes en heap (el ioctl lee 0x60 bytes en +0x00 y escribe 0x20 bytes de clave en +0x60)
    std::vector<std::uint8_t> keybuf(512, 0);
    std::memcpy(keybuf.data(), in_blob, 96);

    const int r = ioctl(pfd, PFS_DECRYPT_KEK_CMD, keybuf.data());
    const int saved_errno = errno;
    close(pfd);

    if (r < 0)
    {
        MACHETE_LOG("[VAULT]", "pfsmgr ioctl failed (ret=%d, errno=%d, hdr=%02x%02x%02x%02x)",
                    r, saved_errno, in_blob[0], in_blob[1], in_blob[2], in_blob[3]);
        return false;
    }

    bool non_zero = false;
    for (std::size_t i = 0; i < 32; ++i)
    {
        if (keybuf[0x60 + i] != 0)
        {
            non_zero = true;
            break;
        }
    }
    if (!non_zero)
    {
        std::int32_t status_80 = 0;
        std::memcpy(&status_80, keybuf.data() + 0x80, sizeof(status_80));
        if (allow_zero_key_for_ps5)
        {
            std::memcpy(out_plain_key, keybuf.data() + 0x60, 32);
            MACHETE_LOG("[VAULT]", "pfsmgr ioctl returned %d for PS5 container (status_0x80=%d); proceeding with kernel buffer at +0x60",
                        r, status_80);
            return true;
        }
        MACHETE_LOG("[VAULT]", "pfsmgr ioctl returned %d but key at +0x60 is zero (status_0x80=%d, hdr=%02x%02x%02x%02x)",
                    r, status_80, in_blob[0], in_blob[1], in_blob[2], in_blob[3]);
        return false;
    }

    std::memcpy(out_plain_key, keybuf.data() + 0x60, 32);
    MACHETE_LOG("[VAULT]", "SAMU decrypted 32-byte PFS AES key via /dev/pfsmgr successfully");
    return true;
}

// Lee y valida un archivo crypto_vault.bin desde una carpeta de respaldo
inline bool read_vault_record(const std::string &backup_folder, CryptoVaultRecord &out_record)
{
    const std::string vpath = backup_folder + "/crypto_vault.bin";
    std::FILE *f = std::fopen(vpath.c_str(), "rb");
    if (!f)
        return false;

    CryptoVaultRecord rec{};
    std::size_t n = std::fread(&rec, 1, sizeof(rec), f);
    std::fclose(f);

    if (n != sizeof(rec) || rec.magic != VAULT_MAGIC)
        return false;

    std::uint8_t expected_hash[32] = {0};
    crypto::Sha256 sha;
    sha.update(reinterpret_cast<const std::uint8_t *>(&rec),
               sizeof(rec) - sizeof(rec.integrity_sha256));
    sha.finish(expected_hash);

    if (std::memcmp(expected_hash, rec.integrity_sha256, 32) != 0)
        return false;

    out_record = rec;
    return true;
}

// Obtiene el OpenPSID de 16 bytes de la consola fisica (via syscall de libkernel o cache del sistema)
inline bool get_console_open_psid(std::uint8_t out_psid[16])
{
    if (!out_psid)
        return false;

    static bool s_cached = false;
    static std::uint8_t s_psid[16] = {0};
    if (s_cached)
    {
        std::memcpy(out_psid, s_psid, 16);
        return true;
    }

#if defined(RTLD_DEFAULT)
    // 1. Invocar la syscall oficial del kernel si el simbolo esta disponible en libkernel
    using GetPsIdFn = int (*)(void *);
    GetPsIdFn psid_fn = reinterpret_cast<GetPsIdFn>(dlsym(RTLD_DEFAULT, "sceKernelGetOpenPsIdForSystem"));
    if (!psid_fn)
        psid_fn = reinterpret_cast<GetPsIdFn>(dlsym(RTLD_DEFAULT, "sceKernelGetOpenPsId"));
    if (psid_fn)
    {
        std::uint8_t sys_psid[16] = {0};
        if (psid_fn(sys_psid) == 0)
        {
            bool non_zero = false;
            for (int i = 0; i < 16; ++i)
            {
                if (sys_psid[i] != 0) { non_zero = true; break; }
            }
            if (non_zero)
            {
                std::memcpy(s_psid, sys_psid, 16);
                std::memcpy(out_psid, sys_psid, 16);
                s_cached = true;
                MACHETE_LOG("[VAULT]", "Console OpenPSID retrieved via libkernel syscall: %02x%02x...%02x%02x (16 bytes)",
                            out_psid[0], out_psid[1], out_psid[14], out_psid[15]);
                return true;
            }
        }
    }
#endif

    // 2. Probar en las rutas de cache del sistema
    for (const char *psid_path : {
             "/system_data/priv/cache/openpsid.dat",
             "/system_data/priv/home/openpsid.dat",
             "/system_data/priv/mms/openpsid.dat"})
    {
        std::FILE *fpsid = std::fopen(psid_path, "rb");
        if (fpsid)
        {
            std::size_t r = std::fread(out_psid, 1, 16, fpsid);
            std::fclose(fpsid);
            if (r == 16)
            {
                std::memcpy(s_psid, out_psid, 16);
                s_cached = true;
                MACHETE_LOG("[VAULT]", "Console OpenPSID retrieved: %02x%02x...%02x%02x (16 bytes)",
                            out_psid[0], out_psid[1], out_psid[14], out_psid[15]);
                return true;
            }
        }
    }
    return false;
}

// Genera y guarda el archivo crypto_vault.bin dentro de la carpeta de respaldo
inline bool archive_crypto_vault(const std::string &dest_folder,
                                 const std::string &title_id,
                                 const std::string &platform,
                                 const std::string &source_save_dir,
                                 std::uint32_t local_user_id)
{
    CryptoVaultRecord record;
    record.local_user_id = local_user_id;
    std::strncpy(record.title_id, title_id.c_str(), sizeof(record.title_id) - 1);
    std::strncpy(record.platform, platform.c_str(), sizeof(record.platform) - 1);

    (void)get_console_open_psid(record.open_psid);

    bool has_sealed = false;

    // 1. Extraer el blob sellado
    if (platform == "PS5" || title_id.rfind("PPSA", 0) == 0)
    {
        // En PS5 native, el blob sellado se encuentra en offset 0x800 dentro de sdimg_*
        // Buscar cualquier sdimg_* en la carpeta de origen
        DIR *dir = opendir(source_save_dir.c_str());
        if (dir)
        {
            while (const dirent *ent = readdir(dir))
            {
                if (std::strncmp(ent->d_name, "sdimg_", 6) == 0)
                {
                    const std::string sd_path = source_save_dir + "/" + ent->d_name;
                    off_t found_off = 0;
                    if (read_ps5_sealed_blob(sd_path, record.sealed_blob, &found_off))
                    {
                        has_sealed = true;
                        MACHETE_LOG("[VAULT]", "Preserved 96-byte SAMU sealed blob from %s (offset 0x%llx)",
                                    sd_path.c_str(), static_cast<unsigned long long>(found_off));
                        break;
                    }
                }
            }
            closedir(dir);
        }
    }
    else
    {
        // En PS4, el sealedkey se encuentra en <name>.bin o en sealedkey en offset 0
        DIR *dir = opendir(source_save_dir.c_str());
        if (dir)
        {
            while (const dirent *ent = readdir(dir))
            {
                const std::string fname = ent->d_name;
                if ((fname.size() > 4 && fname.substr(fname.size() - 4) == ".bin") || fname == "sealedkey")
                {
                    const std::string bin_path = source_save_dir + "/" + fname;
                    if (read_sealed_blob(bin_path, 0, record.sealed_blob))
                    {
                        has_sealed = true;
                        MACHETE_LOG("[VAULT]", "Preserved 96-byte SAMU sealed blob from %s (offset 0x0)", bin_path.c_str());
                        break;
                    }
                }
            }
            closedir(dir);
        }
    }

    // 2. Intentar descifrar la clave AES con SAMU
    if (has_sealed)
    {
        if (decrypt_blob_via_pfsmgr(record.sealed_blob, record.decrypted_pfs_key))
        {
            record.flags |= 1; // Bandera de clave plana disponible
        }
    }

    // 3. Calcular hash de integridad SHA-256
    crypto::Sha256 sha;
    sha.update(reinterpret_cast<const std::uint8_t *>(&record),
               sizeof(record) - sizeof(record.integrity_sha256));
    sha.finish(record.integrity_sha256);

    // 4. Escribir a disco
    const std::string out_path = dest_folder + "/crypto_vault.bin";
    std::FILE *out = std::fopen(out_path.c_str(), "wb");
    if (!out)
    {
        MACHETE_LOG("[VAULT]", "Failed to create crypto vault file at %s (errno=%d)", out_path.c_str(), errno);
        return false;
    }

    std::fwrite(&record, 1, sizeof(record), out);
    std::fflush(out);
    int fd = fileno(out);
    if (fd >= 0)
    {
        fsync(fd);
    }
    std::fclose(out);
    (void)chmod(out_path.c_str(), 0666);

    MACHETE_LOG("[VAULT]", "Cryptographic vault saved successfully: %s (flags=0x%x, platform=%s)",
                out_path.c_str(), record.flags, platform.c_str());
    return true;
}

} // namespace machete::vault
