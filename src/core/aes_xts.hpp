// MacheteSVG - Standalone AES-128-XTS Cipher & PFS In-Place Patcher
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/logger.hpp"
#include "core/sfo_resigner.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace machete::crypto
{

class Aes128
{
  public:
    Aes128() noexcept
    {
        std::memset(round_keys_enc_, 0, sizeof(round_keys_enc_));
    }

    explicit Aes128(const std::uint8_t key[16]) noexcept
    {
        set_key(key);
    }

    void set_key(const std::uint8_t key[16]) noexcept
    {
        key_expansion(key);
    }

    void encrypt_block(const std::uint8_t in[16], std::uint8_t out[16]) const noexcept
    {
        std::uint8_t state[16];
        std::memcpy(state, in, 16);

        add_round_key(state, round_keys_enc_[0]);

        for (int r = 1; r < 10; ++r)
        {
            sub_bytes(state);
            shift_rows(state);
            mix_columns(state);
            add_round_key(state, round_keys_enc_[r]);
        }

        sub_bytes(state);
        shift_rows(state);
        add_round_key(state, round_keys_enc_[10]);

        std::memcpy(out, state, 16);
    }

    void decrypt_block(const std::uint8_t in[16], std::uint8_t out[16]) const noexcept
    {
        std::uint8_t state[16];
        std::memcpy(state, in, 16);

        add_round_key(state, round_keys_enc_[10]);
        inv_shift_rows(state);
        inv_sub_bytes(state);

        for (int r = 9; r >= 1; --r)
        {
            add_round_key(state, round_keys_enc_[r]);
            inv_mix_columns(state);
            inv_shift_rows(state);
            inv_sub_bytes(state);
        }

        add_round_key(state, round_keys_enc_[0]);

        std::memcpy(out, state, 16);
    }

  private:
    std::uint8_t round_keys_enc_[11][16];

    static const std::uint8_t s_box[256];
    static const std::uint8_t inv_s_box[256];
    static const std::uint8_t rcon[10];

    static std::uint8_t xtime(std::uint8_t x) noexcept
    {
        return static_cast<std::uint8_t>((x << 1) ^ (((x >> 7) & 1) * 0x1b));
    }

    static std::uint8_t multiply(std::uint8_t x, std::uint8_t y) noexcept
    {
        std::uint8_t result = 0;
        for (int i = 0; i < 8; ++i)
        {
            if (y & 1)
                result ^= x;
            x = xtime(x);
            y >>= 1;
        }
        return result;
    }

    void key_expansion(const std::uint8_t key[16]) noexcept
    {
        std::memcpy(round_keys_enc_[0], key, 16);

        for (int r = 1; r <= 10; ++r)
        {
            std::uint8_t temp[4];
            temp[0] = round_keys_enc_[r - 1][13];
            temp[1] = round_keys_enc_[r - 1][14];
            temp[2] = round_keys_enc_[r - 1][15];
            temp[3] = round_keys_enc_[r - 1][12];

            temp[0] = s_box[temp[0]] ^ rcon[r - 1];
            temp[1] = s_box[temp[1]];
            temp[2] = s_box[temp[2]];
            temp[3] = s_box[temp[3]];

            for (int i = 0; i < 4; ++i)
                round_keys_enc_[r][i] = round_keys_enc_[r - 1][i] ^ temp[i];

            for (int col = 1; col < 4; ++col)
            {
                for (int i = 0; i < 4; ++i)
                {
                    round_keys_enc_[r][col * 4 + i] =
                        round_keys_enc_[r - 1][col * 4 + i] ^ round_keys_enc_[r][(col - 1) * 4 + i];
                }
            }
        }
    }

    static void add_round_key(std::uint8_t state[16], const std::uint8_t key[16]) noexcept
    {
        for (int i = 0; i < 16; ++i)
            state[i] ^= key[i];
    }

    static void sub_bytes(std::uint8_t state[16]) noexcept
    {
        for (int i = 0; i < 16; ++i)
            state[i] = s_box[state[i]];
    }

    static void inv_sub_bytes(std::uint8_t state[16]) noexcept
    {
        for (int i = 0; i < 16; ++i)
            state[i] = inv_s_box[state[i]];
    }

    static void shift_rows(std::uint8_t state[16]) noexcept
    {
        std::uint8_t t[16];
        std::memcpy(t, state, 16);
        state[0] = t[0];   state[4] = t[4];   state[8] = t[8];   state[12] = t[12];
        state[1] = t[5];   state[5] = t[9];   state[9] = t[13];  state[13] = t[1];
        state[2] = t[10];  state[6] = t[14];  state[10] = t[2];  state[14] = t[6];
        state[3] = t[15];  state[7] = t[3];   state[11] = t[7];  state[15] = t[11];
    }

    static void inv_shift_rows(std::uint8_t state[16]) noexcept
    {
        std::uint8_t t[16];
        std::memcpy(t, state, 16);
        state[0] = t[0];   state[4] = t[4];   state[8] = t[8];   state[12] = t[12];
        state[1] = t[13];  state[5] = t[1];   state[9] = t[5];   state[13] = t[9];
        state[2] = t[10];  state[6] = t[14];  state[10] = t[2];  state[14] = t[6];
        state[3] = t[7];   state[7] = t[11];  state[11] = t[15]; state[15] = t[3];
    }

    static void mix_columns(std::uint8_t state[16]) noexcept
    {
        for (int c = 0; c < 4; ++c)
        {
            const int base = c * 4;
            const std::uint8_t a = state[base + 0];
            const std::uint8_t b = state[base + 1];
            const std::uint8_t c_val = state[base + 2];
            const std::uint8_t d = state[base + 3];

            state[base + 0] = xtime(a ^ b) ^ b ^ c_val ^ d;
            state[base + 1] = xtime(b ^ c_val) ^ c_val ^ d ^ a;
            state[base + 2] = xtime(c_val ^ d) ^ d ^ a ^ b;
            state[base + 3] = xtime(d ^ a) ^ a ^ b ^ c_val;
        }
    }

    static void inv_mix_columns(std::uint8_t state[16]) noexcept
    {
        for (int c = 0; c < 4; ++c)
        {
            const int base = c * 4;
            const std::uint8_t a = state[base + 0];
            const std::uint8_t b = state[base + 1];
            const std::uint8_t c_val = state[base + 2];
            const std::uint8_t d = state[base + 3];

            state[base + 0] = multiply(a, 0x0e) ^ multiply(b, 0x0b) ^ multiply(c_val, 0x0d) ^ multiply(d, 0x09);
            state[base + 1] = multiply(a, 0x09) ^ multiply(b, 0x0e) ^ multiply(c_val, 0x0b) ^ multiply(d, 0x0d);
            state[base + 2] = multiply(a, 0x0d) ^ multiply(b, 0x09) ^ multiply(c_val, 0x0e) ^ multiply(d, 0x0b);
            state[base + 3] = multiply(a, 0x0b) ^ multiply(b, 0x0d) ^ multiply(c_val, 0x09) ^ multiply(d, 0x0e);
        }
    }
};

inline const std::uint8_t Aes128::s_box[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16};

inline const std::uint8_t Aes128::inv_s_box[256] = {
    0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
    0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
    0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
    0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
    0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
    0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
    0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
    0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
    0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
    0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
    0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
    0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
    0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
    0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
    0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d};

inline const std::uint8_t Aes128::rcon[10] = {
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36};

class XtsAes128
{
  public:
    // En Sony PlayStation PFS: tweak_key = key[0..15], data_key = key[16..31]
    // En estandar IEEE P1619: data_key = key[0..15], tweak_key = key[16..31]
    explicit XtsAes128(const std::uint8_t key[32], bool sony_pfs_order = true) noexcept
        : cipher1_(sony_pfs_order ? (key + 16) : key),
          cipher2_(sony_pfs_order ? key : (key + 16))
    {
    }

    static void multiply_alpha(std::uint8_t t[16]) noexcept
    {
        std::uint8_t carry = 0;
        for (int i = 0; i < 16; ++i)
        {
            const std::uint8_t next_carry = (t[i] >> 7) & 1;
            t[i] = static_cast<std::uint8_t>((t[i] << 1) | carry);
            carry = next_carry;
        }
        if (carry)
        {
            t[0] ^= 0x87;
        }
    }

    void decrypt_sector(std::uint64_t sector_index, const std::uint8_t *in, std::uint8_t *out, std::size_t len) const noexcept
    {
        std::uint8_t tweak[16] = {0};
        for (int i = 0; i < 8; ++i)
            tweak[i] = static_cast<std::uint8_t>((sector_index >> (i * 8)) & 0xff);

        std::uint8_t t[16];
        cipher2_.encrypt_block(tweak, t);

        const std::size_t num_blocks = len / 16;
        for (std::size_t b = 0; b < num_blocks; ++b)
        {
            std::uint8_t block[16];
            for (int i = 0; i < 16; ++i)
                block[i] = in[b * 16 + i] ^ t[i];

            std::uint8_t plain[16];
            cipher1_.decrypt_block(block, plain);

            for (int i = 0; i < 16; ++i)
                out[b * 16 + i] = plain[i] ^ t[i];

            multiply_alpha(t);
        }
    }

    void encrypt_sector(std::uint64_t sector_index, const std::uint8_t *in, std::uint8_t *out, std::size_t len) const noexcept
    {
        std::uint8_t tweak[16] = {0};
        for (int i = 0; i < 8; ++i)
            tweak[i] = static_cast<std::uint8_t>((sector_index >> (i * 8)) & 0xff);

        std::uint8_t t[16];
        cipher2_.encrypt_block(tweak, t);

        const std::size_t num_blocks = len / 16;
        for (std::size_t b = 0; b < num_blocks; ++b)
        {
            std::uint8_t block[16];
            for (int i = 0; i < 16; ++i)
                block[i] = in[b * 16 + i] ^ t[i];

            std::uint8_t cipher[16];
            cipher1_.encrypt_block(block, cipher);

            for (int i = 0; i < 16; ++i)
                out[b * 16 + i] = cipher[i] ^ t[i];

            multiply_alpha(t);
        }
    }

  private:
    Aes128 cipher1_;
    Aes128 cipher2_;
};

// Escanea y resigna directamente en el archivo PFS contenedor (ej. Book o savedata.bin)
// los sectores que contienen param.sfo usando la clave plana AES-XTS descifrada por el SAMU
inline bool resign_ps4_pfs_container_sectors(const std::string &image_path,
                                             const std::uint8_t plain_key[32],
                                             const sfo::ResignParams &params,
                                             std::vector<std::uint8_t> *out_extracted_sfo = nullptr)
{
    if (image_path.empty() || !plain_key)
        return false;

    std::FILE *f = std::fopen(image_path.c_str(), "r+b");
    if (!f)
        return false;

    std::fseek(f, 0, SEEK_END);
    const long file_size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);

    if (file_size < 4096)
    {
        std::fclose(f);
        return false;
    }

    constexpr std::size_t kSectorSize = 4096;
    std::vector<std::uint8_t> enc_sector(kSectorSize);
    std::vector<std::uint8_t> dec_sector(kSectorSize);
    const std::uint64_t total_sectors = static_cast<std::uint64_t>(file_size / kSectorSize);

    // Intentar primero con el orden nativo de Sony PFS (tweak primero, data segundo).
    // Si no se encuentra ningun SFO valido, intentar con el orden estandar IEEE P1619.
    const bool order_modes[] = {true, false};

    for (bool sony_order : order_modes)
    {
        XtsAes128 xts(plain_key, sony_order);
        bool sfo_found_and_patched = false;

        for (std::uint64_t sec = 0; sec < total_sectors; ++sec)
        {
            if (std::fseek(f, static_cast<long>(sec * kSectorSize), SEEK_SET) != 0)
                break;

            if (std::fread(enc_sector.data(), 1, kSectorSize, f) != kSectorSize)
                break;

            xts.decrypt_sector(sec, enc_sector.data(), dec_sector.data(), kSectorSize);

            // Buscar SFO_MAGIC (0x46535000 = "\0PSF") en el sector descifrado
            for (std::size_t offset = 0; offset + sizeof(sfo::SfoHeader) <= kSectorSize; offset += 4)
            {
                const auto *hdr = reinterpret_cast<const sfo::SfoHeader *>(dec_sector.data() + offset);
                if (hdr->magic == sfo::SFO_MAGIC && hdr->version == 0x00000101)
                {
                    // Validar que los offsets de tabla caigan dentro de limites razonables
                    if (offset + hdr->key_table_offset < kSectorSize &&
                        offset + hdr->data_table_offset < kSectorSize &&
                        hdr->data_table_offset >= hdr->key_table_offset)
                    {
                        // Comprobar si parece un descriptor de guardado autentico
                        std::vector<std::uint8_t> sfo_buf(dec_sector.begin() + offset, dec_sector.end());
                        sfo::SfoSaveMetadata meta;
                        if (sfo::read_sfo_metadata_from_buffer(sfo_buf.data(), sfo_buf.size(), meta) &&
                            (meta.has_account_id || meta.has_params || meta.category == "sd"))
                        {
                            bool modified = false;
                            if (sfo::resign_param_sfo_buffer(sfo_buf, params, modified) && modified)
                            {
                                // Copiar el SFO resignado de vuelta al sector descifrado
                                std::memcpy(dec_sector.data() + offset, sfo_buf.data(), sfo_buf.size());

                                // Re-encriptar el sector con AES-128-XTS
                                xts.encrypt_sector(sec, dec_sector.data(), enc_sector.data(), kSectorSize);

                                // Escribir el sector re-encriptado directamente en el archivo PFS
                                if (std::fseek(f, static_cast<long>(sec * kSectorSize), SEEK_SET) == 0 &&
                                    std::fwrite(enc_sector.data(), 1, kSectorSize, f) == kSectorSize)
                                {
                                    std::fflush(f);
                                    sfo_found_and_patched = true;
                                    MACHETE_LOG("[PFS]", "Resigned param.sfo in-place inside PFS container sector %llu of %s (mode=%s)",
                                                static_cast<unsigned long long>(sec), image_path.c_str(),
                                                sony_order ? "SonyPFS" : "IEEE");

                                    if (out_extracted_sfo)
                                    {
                                        *out_extracted_sfo = sfo_buf;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        if (sfo_found_and_patched)
        {
            std::fclose(f);
            return true;
        }
    }

    std::fclose(f);
    return false;
}

} // namespace machete::crypto
