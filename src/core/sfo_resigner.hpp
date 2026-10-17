// MacheteSVG - PS4 param.sfo Re-signer and Account ID Activator
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/logger.hpp"
#include "core/sha256_hmac.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace machete::sfo
{

constexpr std::uint32_t SFO_MAGIC = 0x46535000; // "\0PSF"

// Clave HMAC-SHA256 de Sony para la firma del OpenPSID en param.sfo (PS4 / Prospero BC)
inline const std::uint8_t PSID_HMAC_KEY[16] = {
    0x13, 0xD1, 0xDF, 0x06, 0x75, 0xC9, 0xFD, 0x95,
    0x0A, 0x17, 0xE5, 0x64, 0xC2, 0x77, 0x7F, 0x2C};

#pragma pack(push, 1)
struct SfoHeader
{
    std::uint32_t magic;
    std::uint32_t version;
    std::uint32_t key_table_offset;
    std::uint32_t data_table_offset;
    std::uint32_t num_entries;
};

struct SfoIndexEntry
{
    std::uint16_t key_offset;
    std::uint16_t param_format;
    std::uint32_t param_length;
    std::uint32_t param_max_length;
    std::uint32_t data_offset;
};

struct SfoParamsField
{
    std::uint32_t unk1;
    std::uint32_t user_id;       // Offset 0x04: Local User ID (ej. 0x105f615a)
    std::uint8_t psid_hmac[32];  // Offset 0x08: HMAC-SHA256 del OpenPSID
    std::uint32_t unk3;          // Offset 0x28
    char title_id_1[16];         // Offset 0x2C: Title ID principal
    char title_id_2[16];         // Offset 0x3C: Copia de Title ID
    std::uint32_t unk4;          // Offset 0x4C
    std::uint8_t chunk[0x3B0];
};
#pragma pack(pop)

struct ResignParams
{
    std::uint32_t target_user_id = 0;    // User ID local de la PS5 (ej. 0x105f615a)
    std::uint64_t target_account_id = 0; // 64-bit Account ID (0 para cuenta offline no activada)
    bool apply_account_id = true;        // Escribir target_account_id incluso si es 0 (modo Apollo offline)
    const std::uint8_t *psid = nullptr;  // 16 bytes OpenPSID si se conoce
    const std::uint8_t *exact_psid_hmac = nullptr; // 32 bytes psid_hmac extraido de Save Dummy local
};

struct SfoSaveMetadata
{
    bool valid = false;
    std::string category;
    std::string main_title;
    std::string sub_title;
    std::string detail;
    std::string savedata_dir;
    std::uint32_t user_param = 0;
    std::uint64_t blocks = 0;
    std::uint64_t account_id = 0;
    std::uint32_t user_id = 0;
    std::uint8_t psid_hmac[32] = {0};
    bool has_account_id = false;
    bool has_params = false;
    bool has_psid_hmac = false;
};

inline bool read_sfo_metadata(const std::string &sfo_path, SfoSaveMetadata &out)
{
    out = SfoSaveMetadata{};
    std::FILE *in = std::fopen(sfo_path.c_str(), "rb");
    if (!in)
        return false;

    std::fseek(in, 0, SEEK_END);
    const long fsize = std::ftell(in);
    std::fseek(in, 0, SEEK_SET);

    if (fsize <= 0 || fsize > 2 * 1024 * 1024)
    {
        std::fclose(in);
        return false;
    }

    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(fsize));
    const std::size_t read_bytes = std::fread(buffer.data(), 1, buffer.size(), in);
    std::fclose(in);

    if (read_bytes != buffer.size() || buffer.size() < sizeof(SfoHeader))
        return false;

    const auto *header = reinterpret_cast<const SfoHeader *>(buffer.data());
    if (header->magic != SFO_MAGIC)
        return false;

    const std::size_t total_buf_size = buffer.size();
    if (header->key_table_offset >= total_buf_size ||
        header->data_table_offset >= total_buf_size ||
        header->data_table_offset < header->key_table_offset)
        return false;

    const std::size_t entries_bytes = static_cast<std::size_t>(header->num_entries) * sizeof(SfoIndexEntry);
    if (sizeof(SfoHeader) + entries_bytes > header->key_table_offset)
        return false;

    const auto *entries = reinterpret_cast<const SfoIndexEntry *>(buffer.data() + sizeof(SfoHeader));
    const char *key_table = reinterpret_cast<const char *>(buffer.data() + header->key_table_offset);
    const std::size_t key_table_size = header->data_table_offset - header->key_table_offset;
    const std::uint8_t *data_table = buffer.data() + header->data_table_offset;
    const std::size_t data_table_size = total_buf_size - header->data_table_offset;

    auto read_safe_string = [&](const SfoIndexEntry &e) -> std::string {
        const std::size_t rem = data_table_size - e.data_offset;
        const std::size_t len = (e.param_length <= rem) ? e.param_length : rem;
        const char *p = reinterpret_cast<const char *>(data_table + e.data_offset);
        std::size_t slen = 0;
        while (slen < len && p[slen] != '\0')
            ++slen;
        return std::string(p, slen);
    };

    for (std::uint32_t i = 0; i < header->num_entries; ++i)
    {
        if (entries[i].key_offset >= key_table_size)
            continue;
        const char *key = key_table + entries[i].key_offset;
        bool key_null_terminated = false;
        for (std::size_t k = entries[i].key_offset; k < key_table_size; ++k)
        {
            if (key_table[k] == '\0')
            {
                key_null_terminated = true;
                break;
            }
        }
        if (!key_null_terminated || entries[i].data_offset >= data_table_size)
            continue;

        const std::size_t remaining = data_table_size - entries[i].data_offset;
        const std::uint8_t *val_ptr = data_table + entries[i].data_offset;

        if (std::strcmp(key, "CATEGORY") == 0)
        {
            out.category = read_safe_string(entries[i]);
        }
        else if (std::strcmp(key, "MAINTITLE") == 0)
        {
            out.main_title = read_safe_string(entries[i]);
        }
        else if (std::strcmp(key, "SUBTITLE") == 0)
        {
            out.sub_title = read_safe_string(entries[i]);
        }
        else if (std::strcmp(key, "DETAIL") == 0)
        {
            out.detail = read_safe_string(entries[i]);
        }
        else if (std::strcmp(key, "SAVEDATA_DIRECTORY") == 0)
        {
            out.savedata_dir = read_safe_string(entries[i]);
        }
        else if (std::strcmp(key, "SAVEDATA_LIST_PARAM") == 0 && remaining >= 4)
        {
            std::memcpy(&out.user_param, val_ptr, 4);
        }
        else if (std::strcmp(key, "SAVEDATA_BLOCKS") == 0 && remaining >= 8)
        {
            std::memcpy(&out.blocks, val_ptr, 8);
        }
        else if (std::strcmp(key, "SAVEDATA_BLOCKS") == 0 && remaining >= 4)
        {
            std::uint32_t b32 = 0;
            std::memcpy(&b32, val_ptr, 4);
            out.blocks = b32;
        }
        else if (std::strcmp(key, "ACCOUNT_ID") == 0 && entries[i].param_max_length >= 8 && remaining >= 8)
        {
            std::memcpy(&out.account_id, val_ptr, 8);
            out.has_account_id = true;
        }
        else if (std::strcmp(key, "PARAMS") == 0 &&
                 entries[i].param_max_length >= sizeof(SfoParamsField) &&
                 remaining >= sizeof(SfoParamsField))
        {
            const auto *p = reinterpret_cast<const SfoParamsField *>(val_ptr);
            out.user_id = p->user_id;
            std::memcpy(out.psid_hmac, p->psid_hmac, 32);
            out.has_params = true;
            for (int b = 0; b < 32; ++b)
            {
                if (out.psid_hmac[b] != 0)
                {
                    out.has_psid_hmac = true;
                    break;
                }
            }
        }
    }
    out.valid = true;
    return true;
}

inline SfoSaveMetadata read_sfo_metadata(const std::string &sfo_path)
{
    SfoSaveMetadata meta;
    (void)read_sfo_metadata(sfo_path, meta);
    return meta;
}

// Verifica que el param.sfo sea un descriptor de guardado ("sd") y NO un descriptor de juego ("gd")
inline bool is_valid_save_param_sfo(const std::string &sfo_path)
{
    SfoSaveMetadata meta;
    if (!read_sfo_metadata(sfo_path, meta))
        return false;
    if (meta.category == "gd" || meta.category == "gp" || meta.category == "ac")
        return false;
    return (meta.category == "sd" || meta.has_account_id || meta.has_params);
}

inline bool read_account_id_from_sfo(const std::string &sfo_path, std::uint64_t &out_account_id)
{
    SfoSaveMetadata meta;
    if (read_sfo_metadata(sfo_path, meta) && meta.has_account_id)
    {
        out_account_id = meta.account_id;
        return true;
    }
    return false;
}

inline bool resign_param_sfo(const std::string &sfo_path, const ResignParams &params)
{
    std::FILE *in = std::fopen(sfo_path.c_str(), "rb");
    if (!in)
        return false;

    std::fseek(in, 0, SEEK_END);
    const long fsize = std::ftell(in);
    std::fseek(in, 0, SEEK_SET);

    if (fsize <= 0 || fsize > 2 * 1024 * 1024)
    {
        std::fclose(in);
        return false;
    }

    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(fsize));
    const std::size_t read_bytes = std::fread(buffer.data(), 1, buffer.size(), in);
    std::fclose(in);

    if (read_bytes != buffer.size() || buffer.size() < sizeof(SfoHeader))
        return false;

    auto *header = reinterpret_cast<SfoHeader *>(buffer.data());
    if (header->magic != SFO_MAGIC)
        return false;

    const std::size_t total_buf_size = buffer.size();

    // Verificación estricta de límites de tablas contra buffer.size()
    if (header->key_table_offset >= total_buf_size ||
        header->data_table_offset >= total_buf_size ||
        header->data_table_offset < header->key_table_offset)
        return false;

    const std::size_t entries_bytes = static_cast<std::size_t>(header->num_entries) * sizeof(SfoIndexEntry);
    if (sizeof(SfoHeader) + entries_bytes > header->key_table_offset)
        return false;

    auto *entries = reinterpret_cast<SfoIndexEntry *>(buffer.data() + sizeof(SfoHeader));
    const char *key_table = reinterpret_cast<const char *>(buffer.data() + header->key_table_offset);
    const std::size_t key_table_size = header->data_table_offset - header->key_table_offset;
    std::uint8_t *data_table = buffer.data() + header->data_table_offset;
    const std::size_t data_table_size = total_buf_size - header->data_table_offset;

    bool modified = false;

    for (std::uint32_t i = 0; i < header->num_entries; ++i)
    {
        // Validar que la clave no exceda la tabla de claves y esté terminada en nulo
        if (entries[i].key_offset >= key_table_size)
            continue;

        const char *key = key_table + entries[i].key_offset;
        bool key_null_terminated = false;
        for (std::size_t k = entries[i].key_offset; k < key_table_size; ++k)
        {
            if (key_table[k] == '\0') { key_null_terminated = true; break; }
        }
        if (!key_null_terminated)
            continue;

        // Validar que los datos apunten dentro del data_table
        if (entries[i].data_offset >= data_table_size)
            continue;

        std::uint8_t *val_ptr = data_table + entries[i].data_offset;
        const std::size_t remaining_data_space = data_table_size - entries[i].data_offset;

        // 1. Modificar ACCOUNT_ID (8 bytes uint64_t) de forma segura
        if (std::strcmp(key, "ACCOUNT_ID") == 0 &&
            entries[i].param_max_length >= 8 &&
            remaining_data_space >= 8)
        {
            if (params.apply_account_id)
            {
                std::memcpy(val_ptr, &params.target_account_id, 8);
                modified = true;
                MACHETE_LOG("[SFO]", "Patched ACCOUNT_ID -> 0x%04llx...%04llx in %s",
                            static_cast<unsigned long long>((params.target_account_id >> 48) & 0xffff),
                            static_cast<unsigned long long>(params.target_account_id & 0xffff),
                            sfo_path.c_str());
            }
        }

        // 2. Modificar PARAMS (user_id, psid_hmac, title_id_2) de forma segura
        if (std::strcmp(key, "PARAMS") == 0 &&
            entries[i].param_max_length >= sizeof(SfoParamsField) &&
            remaining_data_space >= sizeof(SfoParamsField))
        {
            auto *p = reinterpret_cast<SfoParamsField *>(val_ptr);
            if (params.target_user_id != 0)
            {
                p->user_id = params.target_user_id;
                modified = true;
                MACHETE_LOG("[SFO]", "Patched PARAMS.user_id -> 0x%08x in %s",
                            params.target_user_id, sfo_path.c_str());
            }

            // Sincronizar title_id_2 con title_id_1
            std::memcpy(p->title_id_2, p->title_id_1, sizeof(p->title_id_2));

            // Si se cuenta con el psid_hmac exacto del Save Dummy local, aplicarlo directamente
            if (params.exact_psid_hmac != nullptr)
            {
                std::memcpy(p->psid_hmac, params.exact_psid_hmac, 32);
                modified = true;
            }
            // De lo contrario, si se suministra OpenPSID, recalcular HMAC de Sony
            else if (params.psid != nullptr)
            {
                crypto::hmac_sha256(PSID_HMAC_KEY, sizeof(PSID_HMAC_KEY),
                                    params.psid, 16,
                                    p->psid_hmac);
                modified = true;
            }
            // Si no se suministra OpenPSID nuevo, NO borrar con ceros: preservar el hash existente
        }
    }

    if (!modified)
        return true;

    // Escribir de vuelta de forma segura
    const std::string tmp_path = sfo_path + ".tmp";
    std::FILE *out = std::fopen(tmp_path.c_str(), "wb");
    if (!out)
        return false;

    const std::size_t written = std::fwrite(buffer.data(), 1, buffer.size(), out);
    std::fflush(out);
    int fd = fileno(out);
    if (fd >= 0)
        (void)::fsync(fd);
    std::fclose(out);

    if (written != buffer.size())
    {
        std::remove(tmp_path.c_str());
        return false;
    }

    if (std::rename(tmp_path.c_str(), sfo_path.c_str()) != 0)
    {
        (void)unlink(sfo_path.c_str());
        if (std::rename(tmp_path.c_str(), sfo_path.c_str()) != 0)
        {
            std::remove(tmp_path.c_str());
            return false;
        }
    }
    (void)chmod(sfo_path.c_str(), 0666);
    return true;
}

} // namespace machete::sfo
