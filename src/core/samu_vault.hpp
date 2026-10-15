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

// Descifra el blob sellado a traves del driver de kernel /dev/pfsmgr utilizando SAMU
inline bool decrypt_blob_via_pfsmgr(const std::uint8_t in_blob[96], std::uint8_t out_plain_key[32])
{
    (void)in_blob;
    (void)out_plain_key;
    // Calling raw ioctl on /dev/pfsmgr from unprivileged userland triggers SIGSEGV/kernel traps on stock jailbreaks.
    // The authentic 96-byte hardware-sealed SAMU key blob is safely captured in record.sealed_blob.
    return false;
}

// Obtiene el OpenPSID de 16 bytes de la consola fisica
inline bool get_console_open_psid(std::uint8_t out_psid[16])
{
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
                    if (read_sealed_blob(sd_path, 0x800, record.sealed_blob))
                    {
                        has_sealed = true;
                        MACHETE_LOG("[VAULT]", "Preserved 96-byte SAMU sealed blob from %s (offset 0x800)", sd_path.c_str());
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
