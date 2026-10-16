// MacheteSVG - ISO/IEC 18004 QR Code Generator (Pure C++20, Zero Dependencies)
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

namespace hui::ui::qr
{

// Capacidad maxima de Version 4 (33x33 modulos)
constexpr int kMaxQrSize = 33;
constexpr int kMaxQrModules = kMaxQrSize * kMaxQrSize;

struct QrMatrix
{
    int size = 0; // 21 (V1), 25 (V2), 29 (V3), o 33 (V4)
    std::array<std::uint8_t, kMaxQrModules> modules{};

    bool get(int x, int y) const noexcept
    {
        if (x < 0 || x >= size || y < 0 || y >= size)
            return false;
        return (modules[static_cast<std::size_t>(y * size + x)] & 1) != 0;
    }

    void set(int x, int y, bool black) noexcept
    {
        if (x >= 0 && x < size && y >= 0 && y < size)
            modules[static_cast<std::size_t>(y * size + x)] = black ? 1 : 0;
    }
};

namespace detail
{

// Tablas precalculadas de Galois Field GF(256) (polinomio primitivo 0x11D)
inline constexpr auto make_exp_table()
{
    std::array<std::uint8_t, 512> exp{};
    std::uint16_t x = 1;
    for (int i = 0; i < 255; ++i)
    {
        exp[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(x);
        x = static_cast<std::uint16_t>(x << 1);
        if (x & 0x100)
            x ^= 0x11D;
    }
    for (int i = 255; i < 512; ++i)
    {
        exp[static_cast<std::size_t>(i)] = exp[static_cast<std::size_t>(i - 255)];
    }
    return exp;
}

inline constexpr auto make_log_table()
{
    std::array<std::uint8_t, 256> log{};
    std::uint16_t x = 1;
    for (int i = 0; i < 255; ++i)
    {
        log[static_cast<std::size_t>(x)] = static_cast<std::uint8_t>(i);
        x = static_cast<std::uint16_t>(x << 1);
        if (x & 0x100)
            x ^= 0x11D;
    }
    return log;
}

inline constexpr auto kGfExp = make_exp_table();
inline constexpr auto kGfLog = make_log_table();

inline std::uint8_t gf_mul(std::uint8_t a, std::uint8_t b) noexcept
{
    if (a == 0 || b == 0)
        return 0;
    return kGfExp[static_cast<std::size_t>(kGfLog[a]) + static_cast<std::size_t>(kGfLog[b])];
}

// Parametros para Versiones 1 a 4 en Error Correction Level M
struct VersionInfo
{
    int version;
    int size;
    int data_codewords;
    int ec_codewords;
    int alignment_pattern_pos; // 0 para Version 1
};

inline constexpr VersionInfo kVersions[4] = {
    {1, 21, 16, 10, 0},
    {2, 25, 28, 16, 18},
    {3, 29, 44, 26, 22},
    {4, 33, 64, 36, 26},
};

// Genera los polinomios de correccion de errores Reed-Solomon
inline void rs_encode(const std::uint8_t *data, int data_len, int ec_len, std::uint8_t *out_ec) noexcept
{
    std::vector<std::uint8_t> gen(static_cast<std::size_t>(ec_len + 1), 0);
    gen[0] = 1;
    for (int i = 0; i < ec_len; ++i)
    {
        for (int j = i + 1; j >= 1; --j)
        {
            gen[static_cast<std::size_t>(j)] = gen[static_cast<std::size_t>(j)] ^
                                              gf_mul(gen[static_cast<std::size_t>(j - 1)], kGfExp[static_cast<std::size_t>(i)]);
        }
        gen[0] = gf_mul(gen[0], kGfExp[static_cast<std::size_t>(i)]);
    }

    std::vector<std::uint8_t> rem(static_cast<std::size_t>(ec_len), 0);
    for (int i = 0; i < data_len; ++i)
    {
        std::uint8_t factor = data[i] ^ rem[static_cast<std::size_t>(ec_len - 1)];
        for (int j = ec_len - 1; j >= 1; --j)
        {
            rem[static_cast<std::size_t>(j)] = rem[static_cast<std::size_t>(j - 1)] ^
                                              gf_mul(gen[static_cast<std::size_t>(j)], factor);
        }
        rem[0] = gf_mul(gen[0], factor);
    }

    for (int i = 0; i < ec_len; ++i)
    {
        out_ec[i] = rem[static_cast<std::size_t>(ec_len - 1 - i)];
    }
}

} // namespace detail

// Codifica un texto (ej. URL HTTP) en una matriz QR ISO/IEC 18004
inline bool encode_text(std::string_view text, QrMatrix &out) noexcept
{
    if (text.empty() || text.size() > 62)
        return false;

    // 1. Determinar version minima en ECC Level M
    const detail::VersionInfo *vinfo = nullptr;
    for (const auto &v : detail::kVersions)
    {
        // En Byte mode: 4 bits mode + 8 bits length = 12 bits de overhead (1.5 bytes)
        // Capacidad util = data_codewords - 2 (o -3) bytes
        const int max_bytes = v.data_codewords - 3;
        if (static_cast<int>(text.size()) <= max_bytes)
        {
            vinfo = &v;
            break;
        }
    }

    if (!vinfo)
        return false;

    const int v_size = vinfo->size;
    out.size = v_size;
    out.modules.fill(0);

    std::array<std::uint8_t, kMaxQrModules> reserved{};
    reserved.fill(0);

    auto set_function_module = [&](int x, int y, bool is_black) {
        if (x >= 0 && x < v_size && y >= 0 && y < v_size)
        {
            out.set(x, y, is_black);
            reserved[static_cast<std::size_t>(y * v_size + x)] = 1;
        }
    };

    // 2. Colocar Finder Patterns (7x7) en 3 esquinas
    auto draw_finder = [&](int ox, int oy) {
        for (int dy = -1; dy <= 7; ++dy)
        {
            for (int dx = -1; dx <= 7; ++dx)
            {
                int x = ox + dx;
                int y = oy + dy;
                if (x < 0 || x >= v_size || y < 0 || y >= v_size)
                    continue;

                if (dx >= 0 && dx <= 6 && dy >= 0 && dy <= 6)
                {
                    bool black = (dx == 0 || dx == 6 || dy == 0 || dy == 6 ||
                                  (dx >= 2 && dx <= 4 && dy >= 2 && dy <= 4));
                    set_function_module(x, y, black);
                }
                else
                {
                    set_function_module(x, y, false); // Separador
                }
            }
        }
    };

    draw_finder(0, 0);
    draw_finder(v_size - 7, 0);
    draw_finder(0, v_size - 7);

    // 3. Colocar Alignment Pattern si version >= 2
    if (vinfo->alignment_pattern_pos > 0)
    {
        const int ax = vinfo->alignment_pattern_pos;
        const int ay = vinfo->alignment_pattern_pos;
        for (int dy = -2; dy <= 2; ++dy)
        {
            for (int dx = -2; dx <= 2; ++dx)
            {
                bool black = (std::max(std::abs(dx), std::abs(dy)) != 1);
                set_function_module(ax + dx, ay + dy, black);
            }
        }
    }

    // 4. Timing patterns (fila 6 y columna 6)
    for (int i = 8; i < v_size - 8; ++i)
    {
        set_function_module(i, 6, (i % 2) == 0);
        set_function_module(6, i, (i % 2) == 0);
    }

    // 5. Modulo oscuro fijo
    set_function_module(8, 4 * vinfo->version + 9, true);

    // 6. Reservar areas de informacion de formato (15 bits alrededor de los finders)
    for (int i = 0; i < 9; ++i)
    {
        if (i < v_size)
        {
            reserved[static_cast<std::size_t>(8 * v_size + i)] = 1;
            reserved[static_cast<std::size_t>(i * v_size + 8)] = 1;
        }
    }
    for (int i = 0; i < 8; ++i)
    {
        reserved[static_cast<std::size_t>(8 * v_size + (v_size - 1 - i))] = 1;
        reserved[static_cast<std::size_t>((v_size - 1 - i) * v_size + 8)] = 1;
    }

    // 7. Ensamblar flujo de bits (Byte Mode: 0100)
    std::vector<std::uint8_t> data_codewords;
    data_codewords.reserve(static_cast<std::size_t>(vinfo->data_codewords));

    std::uint32_t bit_buffer = 0;
    int bit_count = 0;

    auto push_bits = [&](std::uint32_t val, int count) {
        bit_buffer = (bit_buffer << count) | (val & ((1U << count) - 1));
        bit_count += count;
        while (bit_count >= 8)
        {
            bit_count -= 8;
            data_codewords.push_back(static_cast<std::uint8_t>((bit_buffer >> bit_count) & 0xFF));
        }
    };

    // Modo Byte: 0100 (4 bits)
    push_bits(0x04, 4);
    // Longitud de caracteres (8 bits para Version 1-9)
    push_bits(static_cast<std::uint32_t>(text.size()), 8);
    // Caracteres en claro
    for (char c : text)
    {
        push_bits(static_cast<std::uint8_t>(c), 8);
    }
    // Terminador (hasta 4 ceros)
    int term_len = std::min(4, (vinfo->data_codewords * 8) - static_cast<int>(data_codewords.size() * 8 + bit_count));
    if (term_len > 0)
        push_bits(0, term_len);
    // Alinear al siguiente byte
    if (bit_count > 0)
        push_bits(0, 8 - bit_count);

    // Padding alternado: 0xEC, 0x11
    std::uint8_t pad = 0xEC;
    while (static_cast<int>(data_codewords.size()) < vinfo->data_codewords)
    {
        data_codewords.push_back(pad);
        pad = (pad == 0xEC) ? 0x11 : 0xEC;
    }

    // 8. Calcular codewords de Error Correction Reed-Solomon
    std::vector<std::uint8_t> ec_codewords(static_cast<std::size_t>(vinfo->ec_codewords), 0);
    detail::rs_encode(data_codewords.data(), vinfo->data_codewords, vinfo->ec_codewords, ec_codewords.data());

    // 9. Combinar Data + EC
    std::vector<std::uint8_t> all_bytes;
    all_bytes.reserve(data_codewords.size() + ec_codewords.size());
    all_bytes.insert(all_bytes.end(), data_codewords.begin(), data_codewords.end());
    all_bytes.insert(all_bytes.end(), ec_codewords.begin(), ec_codewords.end());

    // 10. Ubicar modulos en zig-zag de derecha a izquierda
    int byte_idx = 0;
    int bit_idx = 7;
    int dir = -1; // -1 = hacia arriba, 1 = hacia abajo
    int x = v_size - 1;

    while (x > 0)
    {
        if (x == 6)
            x--; // Saltar columna de sincronizacion

        for (int i = 0; i < v_size; ++i)
        {
            int cy = (dir < 0) ? (v_size - 1 - i) : i;
            for (int dx = 0; dx < 2; ++dx)
            {
                int cx = x - dx;
                if (reserved[static_cast<std::size_t>(cy * v_size + cx)])
                    continue;

                bool bit = false;
                if (byte_idx < static_cast<int>(all_bytes.size()))
                {
                    bit = ((all_bytes[static_cast<std::size_t>(byte_idx)] >> bit_idx) & 1) != 0;
                    bit_idx--;
                    if (bit_idx < 0)
                    {
                        bit_idx = 7;
                        byte_idx++;
                    }
                }

                // Mascara 0: (cx + cy) % 2 == 0
                bool mask = ((cx + cy) % 2 == 0);
                out.set(cx, cy, bit ^ mask);
            }
        }
        x -= 2;
        dir = -dir;
    }

    // 11. Escribir informacion de formato: ECC Level M (00) + Mascara 0 (000) = 00000
    // Con BCH (15, 5) y mascara 0x5412 => 0x429B
    constexpr std::uint16_t kFormatBits = 0x429B;
    for (int i = 0; i < 15; ++i)
    {
        bool bit = ((kFormatBits >> i) & 1) != 0;
        // Alrededor de finder superior izquierdo
        if (i <= 5)
            out.set(8, i, bit);
        else if (i == 6)
            out.set(8, 7, bit);
        else if (i == 7)
            out.set(8, 8, bit);
        else if (i == 8)
            out.set(7, 8, bit);
        else
            out.set(14 - i, 8, bit);

        // Alrededor de los otros dos finders
        if (i < 8)
            out.set(v_size - 1 - i, 8, bit);
        else
            out.set(8, v_size - 15 + i, bit);
    }

    return true;
}

} // namespace hui::ui::qr
