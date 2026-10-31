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

// Formula estandar de Apollo Save Tool para derivar un Account ID sintetico en cuentas offline no activadas
inline std::uint64_t get_apollo_synthetic_account_id(std::uint32_t user_id) noexcept
{
    // Prefijo little-endian correspondiente a la cadena ASCII 'apollo' + complemento a uno del User ID
    const std::uint64_t syn = 0x6F6C6C6F70610000ULL | (static_cast<std::uint64_t>(~user_id & 0xFFFFULL));
    return (syn != 0) ? syn : 0x0100000000000001ULL;
}

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
    std::uint8_t header[8];      // Offset 0x00..0x08: Encabezado/magic interno de PARAMS
    std::uint8_t psid_hmac[32];  // Offset 0x08..0x28: HMAC-SHA256 del OpenPSID (PARAMS+0x08)
    std::uint32_t user_id;       // Offset 0x28..0x2C: Local User ID (ej. 0x105f615a)
    char title_id_1[16];         // Offset 0x2C..0x3C: Title ID principal
    char title_id_2[16];         // Offset 0x3C..0x4C: Copia de Title ID
    std::uint32_t unk4;          // Offset 0x4C..0x50
    std::uint8_t chunk[0x3B0];   // Offset 0x50..0x400: Relleno/datos adicionales (total = 1024 bytes)
};
#pragma pack(pop)

struct ResignParams
{
    std::uint32_t target_user_id = 0;    // User ID local de la PS5 (ej. 0x105f615a)
    std::uint64_t target_account_id = 0; // 64-bit Account ID (0 para cuenta offline no activada)
    bool apply_account_id = true;        // Escribir target_account_id incluso si es 0 (modo Apollo offline)
    const std::uint8_t *psid = nullptr;  // 16 bytes OpenPSID si se conoce
    const std::uint8_t *exact_psid_hmac = nullptr; // 32 bytes psid_hmac extraido de Save Dummy local
    std::string target_title_id;         // Title ID del juego destino para sincronizar title_id_1 y title_id_2
    bool sync_savedata_list_param = false; // Preservar y sincronizar SAVEDATA_LIST_PARAM
    std::uint32_t target_savedata_list_param = 0; // Valor del SAVEDATA_LIST_PARAM a sincronizar
};

struct SfoSaveMetadata
{
    bool valid = false;
    std::string category;
    std::string title_id;
    std::string main_title;
    std::string sub_title;
    std::string detail;
    std::string savedata_dir;
    std::uint32_t user_param = 0;
    std::uint32_t savedata_list_param = 0;
    bool has_savedata_list_param = false;
    std::uint64_t blocks = 0;
    std::uint64_t account_id = 0;
    std::uint32_t user_id = 0;
    std::string title_id_1;
    std::string title_id_2;
    std::uint8_t psid_hmac[32] = {0};
    bool has_account_id = false;
    bool has_params = false;
    bool has_psid_hmac = false;
};

inline bool read_sfo_metadata_from_buffer(const std::uint8_t *data, std::size_t size, SfoSaveMetadata &out)
{
    out = SfoSaveMetadata{};
    if (!data || size < sizeof(SfoHeader))
        return false;

    const auto *header = reinterpret_cast<const SfoHeader *>(data);
    if (header->magic != SFO_MAGIC)
        return false;

    const std::size_t total_buf_size = size;
    if (header->key_table_offset >= total_buf_size ||
        header->data_table_offset >= total_buf_size ||
        header->data_table_offset < header->key_table_offset)
        return false;

    const std::size_t entries_bytes = static_cast<std::size_t>(header->num_entries) * sizeof(SfoIndexEntry);
    if (sizeof(SfoHeader) + entries_bytes > header->key_table_offset)
        return false;

    const auto *entries = reinterpret_cast<const SfoIndexEntry *>(data + sizeof(SfoHeader));
    const char *key_table = reinterpret_cast<const char *>(data + header->key_table_offset);
    const std::size_t key_table_size = header->data_table_offset - header->key_table_offset;
    const std::uint8_t *data_table = data + header->data_table_offset;
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
        else if (std::strcmp(key, "TITLE_ID") == 0)
        {
            out.title_id = read_safe_string(entries[i]);
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
            std::memcpy(&out.savedata_list_param, val_ptr, 4);
            out.has_savedata_list_param = true;
            out.user_param = out.savedata_list_param;
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
                 entries[i].param_max_length >= 0x4C &&
                 remaining >= 0x4C)
        {
            const auto *p = reinterpret_cast<const SfoParamsField *>(val_ptr);
            out.user_id = p->user_id;
            std::memcpy(out.psid_hmac, p->psid_hmac, 32);
            out.title_id_1 = std::string(p->title_id_1, strnlen(p->title_id_1, sizeof(p->title_id_1)));
            out.title_id_2 = std::string(p->title_id_2, strnlen(p->title_id_2, sizeof(p->title_id_2)));
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

    if (read_bytes != buffer.size())
        return false;

    return read_sfo_metadata_from_buffer(buffer.data(), buffer.size(), out);
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
    return (meta.category == "sd" || meta.has_account_id || meta.has_params || !meta.savedata_dir.empty());
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

inline bool resign_param_sfo_buffer(std::vector<std::uint8_t> &buffer, const ResignParams &params, bool &out_modified, const std::string &log_tag = "")
{
    out_modified = false;
    if (buffer.size() < sizeof(SfoHeader))
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
                std::uint64_t final_acc = params.target_account_id;
                if (final_acc == 0)
                {
                    final_acc = get_apollo_synthetic_account_id(params.target_user_id);
                }
                std::memcpy(val_ptr, &final_acc, 8);
                modified = true;
                MACHETE_LOG("[SFO]", "Patched ACCOUNT_ID -> 0x%04llx...%04llx (Apollo standard) in %s",
                            static_cast<unsigned long long>((final_acc >> 48) & 0xffff),
                            static_cast<unsigned long long>(final_acc & 0xffff),
                            log_tag.empty() ? "buffer" : log_tag.c_str());
            }
        }

        // 2. Modificar ATTRIBUTE: Desbloquear candado de proteccion contra copia (Copy-Protection Unlock estilo Apollo)
        if (std::strcmp(key, "ATTRIBUTE") == 0 &&
            entries[i].param_max_length >= 4 &&
            remaining_data_space >= 4)
        {
            std::uint32_t attr = 0;
            std::memcpy(&attr, val_ptr, 4);
            if ((attr & 0x01) != 0)
            {
                attr &= ~0x01u; // Desactivar bit 0 (copy-prohibited) para permitir copiar y restaurar libremente
                std::memcpy(val_ptr, &attr, 4);
                modified = true;
                MACHETE_LOG("[SFO]", "Unlocked copy-protection flag (ATTRIBUTE 0x%x -> 0x%x) in %s",
                            attr | 0x01u, attr, log_tag.empty() ? "buffer" : log_tag.c_str());
            }
        }

        // 3. Modificar TITLE_ID si se especifica target_title_id
        if (!params.target_title_id.empty() &&
            std::strcmp(key, "TITLE_ID") == 0 &&
            entries[i].param_max_length >= params.target_title_id.size() + 1 &&
            remaining_data_space >= params.target_title_id.size() + 1)
        {
            std::memset(val_ptr, 0, entries[i].param_max_length);
            std::memcpy(val_ptr, params.target_title_id.c_str(), params.target_title_id.size());
            entries[i].param_length = static_cast<std::uint32_t>(params.target_title_id.size() + 1);
            modified = true;
            MACHETE_LOG("[SFO]", "Patched TITLE_ID -> '%s' in %s",
                        params.target_title_id.c_str(), log_tag.empty() ? "buffer" : log_tag.c_str());
        }

        // 4. Modificar SAVEDATA_LIST_PARAM si se solicita sincronizacion
        if (params.sync_savedata_list_param &&
            std::strcmp(key, "SAVEDATA_LIST_PARAM") == 0 &&
            entries[i].param_max_length >= 4 &&
            remaining_data_space >= 4)
        {
            std::memcpy(val_ptr, &params.target_savedata_list_param, 4);
            modified = true;
            MACHETE_LOG("[SFO]", "Patched SAVEDATA_LIST_PARAM -> 0x%08x (%u) in %s",
                        params.target_savedata_list_param, params.target_savedata_list_param,
                        log_tag.empty() ? "buffer" : log_tag.c_str());
        }

        // 5. Modificar PARAMS (psid_hmac en PARAMS+0x08, user_id en 0x28, title_id_1 en 0x2c, title_id_2 en 0x3c)
        if (std::strcmp(key, "PARAMS") == 0 &&
            entries[i].param_max_length >= 0x4C &&
            remaining_data_space >= 0x4C)
        {
            auto *p = reinterpret_cast<SfoParamsField *>(val_ptr);

            // (a) Recalcular o inyectar psid_hmac en PARAMS+0x08 (clave 13D1DF0675C9FD950A17E564C2777F2C + OpenPSID local de 16 bytes)
            if (params.exact_psid_hmac != nullptr)
            {
                std::memcpy(p->psid_hmac, params.exact_psid_hmac, 32);
                modified = true;
                MACHETE_LOG("[SFO]", "Patched PARAMS.psid_hmac (at PARAMS+0x08, Save Dummy) -> %02x%02x%02x%02x... in %s",
                            p->psid_hmac[0], p->psid_hmac[1], p->psid_hmac[2], p->psid_hmac[3],
                            log_tag.empty() ? "buffer" : log_tag.c_str());
            }
            else if (params.psid != nullptr)
            {
                crypto::hmac_sha256(PSID_HMAC_KEY, sizeof(PSID_HMAC_KEY),
                                    params.psid, 16,
                                    p->psid_hmac);
                modified = true;
                MACHETE_LOG("[SFO]", "Patched PARAMS.psid_hmac (at PARAMS+0x08, HMAC-SHA256 from OpenPSID) -> %02x%02x%02x%02x... in %s",
                            p->psid_hmac[0], p->psid_hmac[1], p->psid_hmac[2], p->psid_hmac[3],
                            log_tag.empty() ? "buffer" : log_tag.c_str());
            }

            // (b) Modificar user_id en PARAMS+0x28
            if (params.target_user_id != 0)
            {
                p->user_id = params.target_user_id;
                modified = true;
                MACHETE_LOG("[SFO]", "Patched PARAMS.user_id -> 0x%08x in %s",
                            params.target_user_id, log_tag.empty() ? "buffer" : log_tag.c_str());
            }

            // (c) Sincronizar title_id_1 y title_id_2
            if (!params.target_title_id.empty())
            {
                std::memset(p->title_id_1, 0, sizeof(p->title_id_1));
                std::memset(p->title_id_2, 0, sizeof(p->title_id_2));
                std::strncpy(p->title_id_1, params.target_title_id.c_str(), sizeof(p->title_id_1) - 1);
                std::strncpy(p->title_id_2, params.target_title_id.c_str(), sizeof(p->title_id_2) - 1);
                modified = true;
                MACHETE_LOG("[SFO]", "Synced PARAMS.title_id_1/title_id_2 -> '%s' in %s",
                            params.target_title_id.c_str(), log_tag.empty() ? "buffer" : log_tag.c_str());
            }
            else
            {
                std::memcpy(p->title_id_2, p->title_id_1, sizeof(p->title_id_2));
                modified = true;
                MACHETE_LOG("[SFO]", "Synced PARAMS.title_id_2 from title_id_1 ('%s') in %s",
                            p->title_id_1, log_tag.empty() ? "buffer" : log_tag.c_str());
            }
        }
    }

    MACHETE_LOG("[SFO]", "[RESIGN] %s: total_entries=%u, modified=%d",
                log_tag.empty() ? "buffer" : log_tag.c_str(), header->num_entries, modified ? 1 : 0);

    out_modified = modified;
    return true;
}

inline bool resign_param_sfo(const std::string &sfo_path, const ResignParams &params,
                             bool *out_was_modified = nullptr, const std::string &log_tag_override = "")
{
    if (out_was_modified)
        *out_was_modified = false;

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

    if (read_bytes != buffer.size())
        return false;

    const std::string effective_tag = log_tag_override.empty() ? sfo_path : log_tag_override;
    bool modified = false;
    if (!resign_param_sfo_buffer(buffer, params, modified, effective_tag))
        return false;

    if (out_was_modified)
        *out_was_modified = modified;

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

// Genera un descriptor param.sfo de guardado de PS4 autentico (CATEGORY="sd") conforme a la especificacion PSF v1.1
// con claves ordenadas lexicograficamente cuando el respaldo carece de un param.sfo de guardado externo
inline bool synthesize_ps4_save_param_sfo(const std::string &out_sfo_path,
                                          const std::string &title_id,
                                          const std::string &savedata_dir,
                                          const std::string &main_title,
                                          const std::string &sub_title,
                                          const std::string &detail,
                                          std::uint32_t user_id,
                                          std::uint64_t account_id,
                                          std::uint64_t savedata_blocks,
                                          std::uint32_t user_param = 0,
                                          const std::uint8_t *psid = nullptr,
                                          const std::uint8_t *exact_psid_hmac = nullptr)
{
    struct RawEntry
    {
        const char *key;
        std::uint16_t format; // 0x0004 = UTF8-S (binary/struct), 0x0204 = UTF8 null-terminated, 0x0404 = uint32
        std::vector<std::uint8_t> data;
        std::uint32_t max_len;
    };

    auto make_utf8 = [](const char *key, const std::string &val, std::uint32_t max_len) -> RawEntry {
        RawEntry e;
        e.key = key;
        e.format = 0x0204;
        e.max_len = max_len;
        const std::size_t copy_len = (val.size() + 1 <= max_len) ? val.size() : (max_len - 1);
        e.data.assign(max_len, 0);
        if (copy_len > 0)
            std::memcpy(e.data.data(), val.data(), copy_len);
        // param_length incluye el byte nulo terminador
        e.data.resize(copy_len + 1, 0);
        return e;
    };

    auto make_u32 = [](const char *key, std::uint32_t val) -> RawEntry {
        RawEntry e;
        e.key = key;
        e.format = 0x0404;
        e.max_len = 4;
        e.data.resize(4, 0);
        std::memcpy(e.data.data(), &val, 4);
        return e;
    };

    auto make_u64_bin = [](const char *key, std::uint64_t val) -> RawEntry {
        RawEntry e;
        e.key = key;
        e.format = 0x0004;
        e.max_len = 8;
        e.data.resize(8, 0);
        std::memcpy(e.data.data(), &val, 8);
        return e;
    };

    SfoParamsField params_struct{};
    params_struct.user_id = user_id;
    std::strncpy(params_struct.title_id_1, title_id.c_str(), sizeof(params_struct.title_id_1) - 1);
    std::strncpy(params_struct.title_id_2, title_id.c_str(), sizeof(params_struct.title_id_2) - 1);
    if (exact_psid_hmac != nullptr)
    {
        std::memcpy(params_struct.psid_hmac, exact_psid_hmac, 32);
    }
    else if (psid != nullptr)
    {
        crypto::hmac_sha256(PSID_HMAC_KEY, sizeof(PSID_HMAC_KEY), psid, 16, params_struct.psid_hmac);
    }

    RawEntry params_entry;
    params_entry.key = "PARAMS";
    params_entry.format = 0x0004;
    params_entry.max_len = sizeof(SfoParamsField);
    params_entry.data.resize(sizeof(SfoParamsField), 0);
    std::memcpy(params_entry.data.data(), &params_struct, sizeof(SfoParamsField));

    const std::uint64_t blocks_val = (savedata_blocks > 0) ? savedata_blocks : 325;
    const std::string safe_main = main_title.empty() ? title_id : main_title;
    const std::string safe_sub = sub_title.empty() ? savedata_dir : sub_title;

    const std::uint64_t final_acc = (account_id != 0) ? account_id : get_apollo_synthetic_account_id(user_id);

    // Las entradas de un archivo PSF/SFO DEBEN estar en estricto orden ASCII lexicografico por clave
    std::vector<RawEntry> entries;
    entries.push_back(make_u64_bin("ACCOUNT_ID", final_acc));
    entries.push_back(make_u32("ATTRIBUTE", 0));
    entries.push_back(make_utf8("CATEGORY", "sd", 4));
    entries.push_back(make_utf8("DETAIL", detail, 1024));
    entries.push_back(make_utf8("FORMAT", "obs", 4));
    entries.push_back(make_utf8("MAINTITLE", safe_main, 128));
    entries.push_back(params_entry);
    entries.push_back(make_u64_bin("SAVEDATA_BLOCKS", blocks_val));
    entries.push_back(make_utf8("SAVEDATA_DIRECTORY", savedata_dir, 64));
    entries.push_back(make_u32("SAVEDATA_LIST_PARAM", user_param));
    entries.push_back(make_utf8("SUBTITLE", safe_sub, 128));
    entries.push_back(make_utf8("TITLE_ID", title_id, 16));

    std::vector<char> key_table;
    std::vector<std::uint8_t> data_table;
    std::vector<SfoIndexEntry> index_table(entries.size());

    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        index_table[i].key_offset = static_cast<std::uint16_t>(key_table.size());
        const std::size_t klen = std::strlen(entries[i].key) + 1;
        key_table.insert(key_table.end(), entries[i].key, entries[i].key + klen);

        index_table[i].param_format = entries[i].format;
        index_table[i].param_length = static_cast<std::uint32_t>(entries[i].data.size());
        index_table[i].param_max_length = entries[i].max_len;
        index_table[i].data_offset = static_cast<std::uint32_t>(data_table.size());

        data_table.insert(data_table.end(), entries[i].data.begin(), entries[i].data.end());
        if (entries[i].max_len > entries[i].data.size())
        {
            data_table.insert(data_table.end(), entries[i].max_len - entries[i].data.size(), 0);
        }
        while ((data_table.size() % 4) != 0)
            data_table.push_back(0);
    }

    while ((key_table.size() % 4) != 0)
        key_table.push_back('\0');

    SfoHeader hdr{};
    hdr.magic = SFO_MAGIC;
    hdr.version = 0x00000101;
    hdr.key_table_offset = static_cast<std::uint32_t>(sizeof(SfoHeader) + index_table.size() * sizeof(SfoIndexEntry));
    hdr.data_table_offset = static_cast<std::uint32_t>(hdr.key_table_offset + key_table.size());
    hdr.num_entries = static_cast<std::uint32_t>(index_table.size());

    const std::string tmp_path = out_sfo_path + ".tmp";
    std::FILE *out = std::fopen(tmp_path.c_str(), "wb");
    if (!out)
        return false;

    bool ok = true;
    if (std::fwrite(&hdr, 1, sizeof(hdr), out) != sizeof(hdr))
        ok = false;
    if (ok && std::fwrite(index_table.data(), sizeof(SfoIndexEntry), index_table.size(), out) != index_table.size())
        ok = false;
    if (ok && std::fwrite(key_table.data(), 1, key_table.size(), out) != key_table.size())
        ok = false;
    if (ok && std::fwrite(data_table.data(), 1, data_table.size(), out) != data_table.size())
        ok = false;

    std::fflush(out);
    int fd = fileno(out);
    if (fd >= 0)
        (void)::fsync(fd);
    std::fclose(out);

    if (!ok)
    {
        (void)unlink(tmp_path.c_str());
        return false;
    }

    if (std::rename(tmp_path.c_str(), out_sfo_path.c_str()) != 0)
    {
        (void)unlink(out_sfo_path.c_str());
        if (std::rename(tmp_path.c_str(), out_sfo_path.c_str()) != 0)
        {
            (void)unlink(tmp_path.c_str());
            return false;
        }
    }
    (void)chmod(out_sfo_path.c_str(), 0666);
    MACHETE_LOG("[SFO]", "Synthesized authentic PS4 Save Data param.sfo (CATEGORY='sd', slot='%s', blocks=%llu) at %s",
                savedata_dir.c_str(), static_cast<unsigned long long>(blocks_val), out_sfo_path.c_str());
    return true;
}

} // namespace machete::sfo
