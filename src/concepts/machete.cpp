// MacheteSVG - PS5 & PS4 Savegame Manager & Web Save Station
// Powered by ps5-homebrew-ui & PS5 Payload SDK
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "concepts/concepts.hpp"

#include "core/build_info.hpp"
#include "core/game_scanner.hpp"
#include "core/i18n.hpp"
#include "core/jailbreak.hpp"
#include "core/logger.hpp"
#include "core/save_manager.hpp"
#include "core/storage.hpp"
#include "core/tween.hpp"
#include "core/worker_thread.hpp"
#include "demo/catalog.hpp"
#include "server/web_server.hpp"
#include "ui/glyphs.hpp"
#include "ui/motion.hpp"
#include "ui/theme.hpp"
#include "ui/qr_code.hpp"
#include "ui/widgets.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

namespace hui::concepts
{

namespace
{

using gfx::Color;
using gfx::Rect;

// Pestañas de filtrado de plataforma
enum class GameTab : int
{
    PS5 = 0,
    PS4 = 1,
    All = 2
};

// Estados del personaje animado anime Shin-chan
enum class ShinchanState
{
    Hidden,
    Dancing,
    Success,
    Error
};

// Identificadores de elementos navegables
enum Item : int
{
    // Pestañas superiores del panel izquierdo
    kTabPS5 = 0,
    kTabPS4,
    kTabAll,

    // Panel Izquierdo: Lista visible de juegos (hasta 5 en pantalla)
    kGame0,
    kGame1,
    kGame2,
    kGame3,
    kGame4,

    // Panel Derecho: Botones de acción
    kBtnBackup,
    kBtnRestore,
    kBtnWebServer,

    // Cabecera: Botón / Chip de Servidor Web
    kBtnWebServerHeader,

    kItemCount
};

// Rectángulos de foco base
constexpr Rect kRects[kItemCount] = {
    // Pestañas superiores (panel izquierdo)
    {116, 266, 206, 42}, // PS5
    {332, 266, 206, 42}, // PS4
    {548, 266, 208, 42}, // TODOS

    // Lista de juegos (panel izquierdo)
    {116, 330, 640, 92},
    {116, 434, 640, 92},
    {116, 538, 640, 92},
    {116, 642, 640, 92},
    {116, 746, 640, 92},

    // Botones de acción (panel derecho)
    {836, 780, 260, 64},  // Respaldar (Primary)
    {1112, 780, 260, 64}, // Restaurar (Secondary)
    {836, 856, 536, 64},  // Estacion Web Local (Secondary)

    // Chip Servidor Web en Cabecera
    {1540, 108, 284, 44},
};

static constexpr std::array<const char *const, 4> kTechniques = {
    "Core-Shell Architecture: Complete decoupling between cryptographic engine and GUI",
    "Keystone Preservation: Protects /sce_sys/ to prevent error CE-108255-1",
    "Atomic POSIX Writes: Safe replacements with vnode memory synchronization",
    "Web Save Station: Embedded HTTP server with live ZIP streaming for PC/Mobile browser downloads",
};

// Renderizado nativo a 60 FPS del personaje Shin-chan (soporte para 12 fotogramas del GIF canonico + fallback vectorial)
static void draw_shinchan_character(gfx::DrawList &list, ui::Painter &paint,
                                    float cx, float cy, float clock, float opacity,
                                    ShinchanState state,
                                    const std::array<unsigned int, 12> *dance_textures = nullptr)
{
    if (state == ShinchanState::Hidden || opacity <= 0.005f)
        return;

    list.push_opacity(opacity);

    // Paleta oficial canonica de Shin-chan
    const Color skin_color = Color::rgb(0xffdfbe);
    const Color skin_shadow = Color::rgb(0xf2bc96);
    const Color hair_color = Color::rgb(0x1a1a1a);
    const Color eyebrow_color = Color::rgb(0x111111);
    const Color red_shirt = Color::rgb(0xe52521);
    const Color yellow_shorts = Color::rgb(0xffdf00);
    const Color white_socks = Color::rgb(0xffffff);
    const Color yellow_shoes = Color::rgb(0xffca00);
    const Color blush_color = Color::rgb(0xff7582);

    if (state == ShinchanState::Dancing)
    {
        // 1. Si disponemos de las texturas de los 12 fotogramas del GIF de Shin-chan, reproducir a 10 FPS
        const int frame_idx = static_cast<int>(clock * 10.0f) % 12;
        if (dance_textures && (*dance_textures)[static_cast<std::size_t>(frame_idx)] != 0)
        {
            // Sombra suave en el suelo
            list.shadow({cx - 40.0f, cy - 2.0f, 80.0f, 12.0f}, 6.0f, 5.0f,
                        Color::rgb(0x000000).with_alpha(0.30f));

            // Renderizado del fotograma transparente canonico (proporcion original 370x300)
            const float w = 154.0f;
            const float h = 125.0f;
            const Rect dst{cx - w * 0.5f, cy - h - 5.0f, w, h};
            list.image((*dance_textures)[static_cast<std::size_t>(frame_idx)], dst,
                       gfx::kFullUv, Color::rgb(0xffffff));

            list.pop_opacity();
            return;
        }

        // 2. Fallback vectorial canonico
        const float tempo = clock * 7.5f;
        const float sway = std::sin(tempo) * 16.0f;
        const float tilt = std::sin(tempo) * 0.12f;
        const float bounce = std::abs(std::sin(tempo * 2.0f)) * 5.0f;
        const float arm_swing = std::sin(tempo) * 22.0f;

        // Sombra suave proyectada en el suelo
        list.shadow({cx - 36.0f + sway * 0.3f, cy - 2.0f, 72.0f, 12.0f}, 6.0f, 5.0f,
                    Color::rgb(0x000000).with_alpha(0.35f));

        // Piernas infantiles con paso ritmico
        const float leg_l_y = cy - 20.0f + (sway > 0 ? bounce : 0.0f);
        const float leg_r_y = cy - 20.0f + (sway < 0 ? bounce : 0.0f);

        list.line(cx - 14.0f + sway * 0.4f, cy - 30.0f + bounce,
                  cx - 16.0f + sway * 0.2f, leg_l_y, 7.0f, skin_color);
        list.line(cx + 14.0f + sway * 0.4f, cy - 30.0f + bounce,
                  cx + 16.0f + sway * 0.2f, leg_r_y, 7.0f, skin_color);

        // Calcetines blancos
        list.rounded_rect({cx - 21.0f + sway * 0.2f, leg_l_y - 4.0f, 10.0f, 5.0f}, 2.0f, white_socks);
        list.rounded_rect({cx + 11.0f + sway * 0.2f, leg_r_y - 4.0f, 10.0f, 5.0f}, 2.0f, white_socks);

        // Zapatillas amarillas clasicas de Shin-chan con puntera levantada
        list.rounded_rect({cx - 24.0f + sway * 0.2f, leg_l_y - 1.0f, 15.0f, 6.0f}, 3.0f, yellow_shoes);
        list.rounded_rect({cx + 9.0f + sway * 0.2f, leg_r_y - 1.0f, 15.0f, 6.0f}, 3.0f, yellow_shoes);
        // Suela fina
        list.line(cx - 24.0f + sway * 0.2f, leg_l_y + 5.0f, cx - 10.0f + sway * 0.2f, leg_l_y + 5.0f, 1.2f, Color::rgb(0x333333));
        list.line(cx + 9.0f + sway * 0.2f, leg_r_y + 5.0f, cx + 23.0f + sway * 0.2f, leg_r_y + 5.0f, 1.2f, Color::rgb(0x333333));

        // Pantalones cortos amarillos canonicos
        const float hip_x = cx + sway * 0.6f;
        const float hip_y = cy - 42.0f + bounce;
        list.rounded_rect({hip_x - 22.0f, hip_y, 44.0f, 18.0f}, 7.0f, yellow_shorts);
        // Pliegue central del pantalon
        list.line(hip_x, hip_y + 6.0f, hip_x, hip_y + 17.0f, 1.8f, skin_shadow);

        // Camiseta roja de manga larga de Shin-chan
        const float torso_x = cx + sway * 0.75f;
        const float torso_y = cy - 70.0f + bounce;
        list.rounded_rect({torso_x - 24.0f, torso_y, 48.0f, 32.0f}, 9.0f, red_shirt);
        // Cuello redondo de la camiseta
        list.arc(torso_x, torso_y + 3.0f, 7.0f, 2.0f, 0.2f, 2.7f, Color::rgb(0xb51210));

        // Brazos oscilantes en el baile (un brazo al frente, otro lateral como en imagen 2 y 3)
        // Brazo izquierdo (cruzado hacia la derecha o balanceandose)
        list.line(torso_x - 18.0f, torso_y + 8.0f,
                  torso_x + arm_swing * 0.5f - 4.0f, torso_y + 14.0f + bounce * 0.5f, 6.5f, red_shirt);
        list.circle(torso_x + arm_swing * 0.5f - 4.0f, torso_y + 14.0f + bounce * 0.5f, 4.5f, skin_color);

        // Brazo derecho (extendido lateralmente en direccion contraria)
        list.line(torso_x + 18.0f, torso_y + 8.0f,
                  torso_x + 36.0f - arm_swing * 0.6f, torso_y + 10.0f - bounce * 0.5f, 6.5f, red_shirt);
        list.circle(torso_x + 36.0f - arm_swing * 0.6f, torso_y + 10.0f - bounce * 0.5f, 4.5f, skin_color);

        // Cabeza canonica con silueta de patata y moflete rollizo prominente
        const float head_x = cx + sway * 0.9f;
        const float head_y = cy - 95.0f + bounce;

        // Base craneal y gran moflete lateral (silueta tipica de Shin-chan)
        list.circle(head_x - 4.0f, head_y, 22.0f, skin_color);
        list.circle(head_x + 15.0f, head_y + 5.0f, 16.0f, skin_color);
        list.circle(head_x + 8.0f, head_y - 2.0f, 18.0f, skin_color);

        // Oreja redonda grande con pliegue interno (lado izquierdo/trasero)
        list.circle(head_x - 22.0f, head_y + 3.0f, 7.5f, skin_color);
        list.arc(head_x - 22.0f, head_y + 3.0f, 4.5f, 1.6f, 0.5f, 3.8f, skin_shadow);

        // Pelo negro canonico con corte ondulado caracteristico
        list.rounded_rect({head_x - 24.0f, head_y - 26.0f, 38.0f, 16.0f}, 6.0f, hair_color);
        list.circle(head_x - 12.0f, head_y - 14.0f, 16.0f, hair_color);
        list.circle(head_x + 4.0f, head_y - 16.0f, 14.0f, hair_color);
        // Perfilado del corte de pelo sobre la frente
        list.circle(head_x + 6.0f, head_y - 4.0f, 14.0f, skin_color);

        // Cejas gruesas oruga inconfundibles de Shin-chan (muy pobladas y arqueadas)
        list.rounded_rect({head_x - 12.0f, head_y - 12.0f + tilt * 10.0f, 13.0f, 5.0f}, 2.5f, eyebrow_color);
        list.rounded_rect({head_x + 4.0f, head_y - 11.0f - tilt * 10.0f, 15.0f, 5.2f}, 2.5f, eyebrow_color);

        // Ojos relajados en lineas curvas de felicidad y concentracion en el baile (como imagen 2 y 3)
        list.line(head_x - 8.0f, head_y - 1.0f, head_x - 1.0f, head_y - 1.0f, 2.2f, eyebrow_color);
        list.line(head_x + 7.0f, head_y, head_x + 16.0f, head_y, 2.2f, eyebrow_color);

        // Boca pequena ovalada cantando / silbando alegremente ("O")
        list.circle(head_x + 15.0f, head_y + 10.0f, 4.5f, Color::rgb(0x7a0c0a));
        list.circle(head_x + 15.0f, head_y + 11.0f, 2.5f, Color::rgb(0xff6e79));

        // Lineas de rubor rosado en el moflete (///)
        list.line(head_x + 17.0f, head_y + 4.0f, head_x + 19.0f, head_y + 7.0f, 1.5f, blush_color);
        list.line(head_x + 20.0f, head_y + 4.0f, head_x + 22.0f, head_y + 7.0f, 1.5f, blush_color);
        list.line(head_x + 23.0f, head_y + 4.0f, head_x + 25.0f, head_y + 7.0f, 1.5f, blush_color);

        // Notas musicales flotantes estilizadas
        const float note_phase = std::fmod(clock * 2.0f, 1.0f);
        const float note_y = head_y - 20.0f - note_phase * 22.0f;
        const float note_alpha = 1.0f - note_phase;
        list.circle(head_x + 30.0f, note_y, 3.0f, Color::rgb(0xffd54f).with_alpha(note_alpha));
        list.line(head_x + 33.0f, note_y, head_x + 33.0f, note_y - 10.0f, 1.5f, Color::rgb(0xffd54f).with_alpha(note_alpha));
    }
    else if (state == ShinchanState::Success)
    {
        // Celebracion festiva de victoria canonica (estilo imagen 5, alegre con cartel de checkmark)
        const float hop = std::abs(std::sin(clock * 7.0f)) * 10.0f;

        list.shadow({cx - 36.0f, cy - 2.0f, 72.0f, 12.0f}, 6.0f, 5.0f,
                    Color::rgb(0x000000).with_alpha(0.35f));

        // Piernas infantiles saltando alegremente
        list.line(cx - 13.0f, cy - 24.0f - hop, cx - 18.0f, cy - hop, 7.0f, skin_color);
        list.line(cx + 13.0f, cy - 24.0f - hop, cx + 18.0f, cy - hop, 7.0f, skin_color);
        list.rounded_rect({cx - 23.0f, cy - 6.0f - hop, 10.0f, 5.0f}, 2.0f, white_socks);
        list.rounded_rect({cx + 13.0f, cy - 6.0f - hop, 10.0f, 5.0f}, 2.0f, white_socks);
        list.rounded_rect({cx - 26.0f, cy - 2.0f - hop, 15.0f, 6.0f}, 3.0f, yellow_shoes);
        list.rounded_rect({cx + 11.0f, cy - 2.0f - hop, 15.0f, 6.0f}, 3.0f, yellow_shoes);

        // Pantalones cortos amarillos
        list.rounded_rect({cx - 22.0f, cy - 42.0f - hop, 44.0f, 18.0f}, 7.0f, yellow_shorts);

        // Camiseta roja de manga larga
        list.rounded_rect({cx - 24.0f, cy - 70.0f - hop, 48.0f, 32.0f}, 9.0f, red_shirt);

        // Brazos en alto sosteniendo el cartel de victoria
        list.line(cx - 18.0f, cy - 60.0f - hop, cx - 26.0f, cy - 90.0f - hop, 6.5f, red_shirt);
        list.circle(cx - 26.0f, cy - 91.0f - hop, 4.5f, skin_color);
        list.line(cx + 18.0f, cy - 60.0f - hop, cx + 26.0f, cy - 90.0f - hop, 6.5f, red_shirt);
        list.circle(cx + 26.0f, cy - 91.0f - hop, 4.5f, skin_color);

        // Cabeza alegre con silueta de patata y gran moflete
        const float head_y = cy - 92.0f - hop;
        list.circle(cx - 4.0f, head_y, 22.0f, skin_color);
        list.circle(cx + 15.0f, head_y + 4.0f, 15.0f, skin_color);
        list.circle(cx + 8.0f, head_y - 2.0f, 18.0f, skin_color);

        // Oreja
        list.circle(cx - 22.0f, head_y + 3.0f, 7.5f, skin_color);
        list.arc(cx - 22.0f, head_y + 3.0f, 4.5f, 1.6f, 0.5f, 3.8f, skin_shadow);

        // Pelo negro
        list.rounded_rect({cx - 24.0f, head_y - 26.0f, 38.0f, 16.0f}, 6.0f, hair_color);
        list.circle(cx - 12.0f, head_y - 14.0f, 16.0f, hair_color);
        list.circle(cx + 4.0f, head_y - 16.0f, 14.0f, hair_color);
        list.circle(cx + 6.0f, head_y - 4.0f, 14.0f, skin_color);

        // Cejas gruesas oruga arqueadas alegres
        list.rounded_rect({cx - 12.0f, head_y - 14.0f, 13.0f, 5.0f}, 2.5f, eyebrow_color);
        list.rounded_rect({cx + 4.0f, head_y - 13.0f, 15.0f, 5.2f}, 2.5f, eyebrow_color);

        // Ojo grande brillante expresivo (estilo imagen 5)
        list.circle(cx + 9.0f, head_y + 1.0f, 6.5f, eyebrow_color);
        list.circle(cx + 11.0f, head_y - 1.0f, 2.5f, Color::rgb(0xffffff));

        // Guiño o ojo arqueado izquierdo
        list.arc(cx - 4.0f, head_y + 1.0f, 5.5f, 2.2f, -1.2f, 2.4f, eyebrow_color);

        // Boca abierta sonriente con lengua rosada
        list.circle(cx + 14.0f, head_y + 10.0f, 7.0f, Color::rgb(0x7a0c0a));
        list.circle(cx + 14.0f, head_y + 12.0f, 4.5f, Color::rgb(0xff6e79));

        // Lineas de rubor rosado en las mejillas (///)
        list.line(cx + 18.0f, head_y + 4.0f, cx + 20.0f, head_y + 7.0f, 1.5f, blush_color);
        list.line(cx + 21.0f, head_y + 4.0f, cx + 23.0f, head_y + 7.0f, 1.5f, blush_color);
        list.line(cx + 24.0f, head_y + 4.0f, cx + 26.0f, head_y + 7.0f, 1.5f, blush_color);

        // Cartel sostenido con checkmark verde
        const Rect sign{cx - 56.0f, cy - 148.0f - hop, 112.0f, 48.0f};
        list.rounded_rect(sign, 9.0f, Color::rgb(0x062817).with_alpha(0.95f));
        paint.stroke(sign, 9.0f, 1.8f, Color::rgb(0x38ef7d));

        // Circulo con checkmark verde
        list.circle(sign.x + 22.0f, sign.cy(), 13.0f, Color::rgb(0x114626));
        list.line(sign.x + 15.0f, sign.cy(), sign.x + 20.0f, sign.cy() + 5.0f, 3.0f, Color::rgb(0x38ef7d));
        list.line(sign.x + 20.0f, sign.cy() + 5.0f, sign.x + 29.0f, sign.cy() - 6.0f, 3.0f, Color::rgb(0x38ef7d));

        const bool is_es = (machete::i18n::current_language() == machete::i18n::Language::Spanish);
        paint.heading(is_es ? "EXITO" : "DONE", sign.x + 42.0f, sign.cy() + 6.5f, 17.0f, Color::rgb(0x38ef7d));

        // Estrellas doradas de celebracion
        list.star(cx - 62.0f, cy - 116.0f - hop, 7.0f, Color::rgb(0xffd700));
        list.star(cx + 62.0f, cy - 116.0f - hop, 7.0f, Color::rgb(0xffd700));
    }
    else if (state == ShinchanState::Error)
    {
        // Estado de fallo/error con lagrimas animadas y cartel con X roja
        const float shake = std::sin(clock * 24.0f) * 2.2f;

        list.shadow({cx - 36.0f, cy - 2.0f, 72.0f, 12.0f}, 6.0f, 5.0f,
                    Color::rgb(0x000000).with_alpha(0.35f));

        // Charco de lagrimas azul
        list.rounded_rect({cx - 34.0f, cy + 2.0f, 68.0f, 5.0f}, 2.5f,
                          Color::rgb(0x29b6f6).with_alpha(0.60f));

        // Piernas
        list.line(cx - 13.0f, cy - 22.0f, cx - 17.0f, cy, 7.0f, skin_color);
        list.line(cx + 13.0f, cy - 22.0f, cx + 17.0f, cy, 7.0f, skin_color);
        list.rounded_rect({cx - 23.0f, cy - 5.0f, 10.0f, 5.0f}, 2.0f, white_socks);
        list.rounded_rect({cx + 11.0f, cy - 5.0f, 10.0f, 5.0f}, 2.0f, white_socks);
        list.rounded_rect({cx - 25.0f, cy - 2.0f, 15.0f, 6.0f}, 3.0f, yellow_shoes);
        list.rounded_rect({cx + 10.0f, cy - 2.0f, 15.0f, 6.0f}, 3.0f, yellow_shoes);

        // Pantalones cortos amarillos temblorosos
        list.rounded_rect({cx - 22.0f + shake, cy - 40.0f, 44.0f, 18.0f}, 7.0f, yellow_shorts);

        // Camiseta roja
        list.rounded_rect({cx - 24.0f + shake, cy - 68.0f, 48.0f, 32.0f}, 9.0f, red_shirt);

        // Brazos abatidos sosteniendo el cartel
        list.line(cx - 18.0f + shake, cy - 58.0f, cx - 26.0f, cy - 88.0f, 6.5f, red_shirt);
        list.circle(cx - 26.0f, cy - 89.0f, 4.5f, skin_color);
        list.line(cx + 18.0f + shake, cy - 58.0f, cx + 26.0f, cy - 88.0f, 6.5f, red_shirt);
        list.circle(cx + 26.0f, cy - 89.0f, 4.5f, skin_color);

        // Cabeza abatida con silueta de patata
        const float head_y = cy - 88.0f;
        list.circle(cx - 4.0f + shake, head_y, 22.0f, skin_color);
        list.circle(cx + 15.0f + shake, head_y + 4.0f, 15.0f, skin_color);
        list.circle(cx + 8.0f + shake, head_y - 2.0f, 18.0f, skin_color);

        // Pelo
        list.rounded_rect({cx - 24.0f + shake, head_y - 26.0f, 38.0f, 16.0f}, 6.0f, hair_color);
        list.circle(cx - 12.0f + shake, head_y - 14.0f, 16.0f, hair_color);
        list.circle(cx + 4.0f + shake, head_y - 16.0f, 14.0f, hair_color);
        list.circle(cx + 6.0f + shake, head_y - 4.0f, 14.0f, skin_color);

        // Cejas gruesas caidas temblorosas (\ /)
        list.line(cx - 12.0f + shake, head_y - 9.0f, cx - 1.0f + shake, head_y - 5.0f, 4.5f, eyebrow_color);
        list.line(cx + 16.0f + shake, head_y - 9.0f, cx + 5.0f + shake, head_y - 5.0f, 4.5f, eyebrow_color);

        // Ojos llorosos apretados
        list.arc(cx - 5.0f + shake, head_y + 1.0f, 6.0f, 2.5f, 0.2f, 2.7f, eyebrow_color);
        list.arc(cx + 11.0f + shake, head_y + 1.0f, 6.0f, 2.5f, 0.2f, 2.7f, eyebrow_color);

        // Cascadas de lagrimas animadas estilo anime
        const float tear_flow = std::fmod(clock * 130.0f, 44.0f);
        list.line(cx - 6.0f + shake, head_y + 2.0f, cx - 18.0f + shake, cy - 10.0f, 4.0f,
                  Color::rgb(0x29b6f6).with_alpha(0.85f));
        list.circle(cx - 14.0f + shake, head_y + 6.0f + tear_flow, 4.0f, Color::rgb(0x81d4fa));

        list.line(cx + 14.0f + shake, head_y + 2.0f, cx + 24.0f + shake, cy - 10.0f, 4.0f,
                  Color::rgb(0x29b6f6).with_alpha(0.85f));
        list.circle(cx + 20.0f + shake, head_y + 6.0f + tear_flow, 4.0f, Color::rgb(0x81d4fa));

        // Boca temblorosa en arco invertido
        list.arc(cx + 8.0f + shake, head_y + 12.0f, 7.5f, 2.8f, 2.4f, 1.8f, Color::rgb(0x3a0d12));

        // Cartel sostenido con X roja
        const Rect sign{cx - 56.0f, cy - 148.0f, 112.0f, 48.0f};
        list.rounded_rect(sign, 9.0f, Color::rgb(0x27070a).with_alpha(0.95f));
        paint.stroke(sign, 9.0f, 1.8f, Color::rgb(0xff4455));

        // Circulo con X roja
        list.circle(sign.x + 22.0f, sign.cy(), 13.0f, Color::rgb(0x480e14));
        list.line(sign.x + 17.0f, sign.cy() - 5.5f, sign.x + 27.0f, sign.cy() + 5.5f, 3.0f, Color::rgb(0xff4455));
        list.line(sign.x + 27.0f, sign.cy() - 5.5f, sign.x + 17.0f, sign.cy() + 5.5f, 3.0f, Color::rgb(0xff4455));

        const bool is_es = (machete::i18n::current_language() == machete::i18n::Language::Spanish);
        paint.heading(is_es ? "FALLO" : "ERROR", sign.x + 42.0f, sign.cy() + 6.5f, 17.0f, Color::rgb(0xff4455));
    }

    list.pop_opacity();
}

class Machete final : public app::Concept
{
  public:
    explicit Machete(app::Context &context) : context_(context)
    {
        machete::i18n::init();
        current_tab_ = GameTab::PS5;
        focus_ = kGame0;
        selected_game_ = 0;
        tab_indicator_.snap(0.0f);
        focus_amount_[kGame0].snap(1.0f);
        op_progress_bar_.snap(0.0f);
        op_overlay_show_.snap(0.0f);
        build_tour();
    }

    ~Machete() override
    {
        if (op_thread_.joinable())
            op_thread_.join();
        if (jb_thread_.joinable())
            jb_thread_.join();
        machete::server::WebServer::instance().stop();
        machete::scanner::release_game_icon_textures(games_);
        if (app_icon_texture_ != 0)
        {
            machete::scanner::release_texture(app_icon_texture_);
            app_icon_texture_ = 0;
        }
        for (auto &tex : shinchan_dance_textures_)
        {
            if (tex != 0)
            {
                machete::scanner::release_texture(tex);
                tex = 0;
            }
        }
    }

    void load_app_icon() const
    {
        if (app_icon_attempted_ || app_icon_texture_ != 0)
            return;
        app_icon_attempted_ = true;

        const char *candidates[] = {
            "/app0/sce_sys/icon0.png",
            "sce_sys/icon0.png",
            "/data/homebrew/PPSA99001/sce_sys/icon0.png",
            "/user/appmeta/PPSA99001/icon0.png",
            "/mnt/ext0/user/appmeta/PPSA99001/icon0.png",
            "/mnt/ext1/user/appmeta/PPSA99001/icon0.png",
        };

        for (const char *path : candidates)
        {
            app_icon_texture_ = machete::scanner::load_texture_from_png(path);
            if (app_icon_texture_ != 0)
                break;
        }
    }

    void load_shinchan_textures() const
    {
        if (shinchan_textures_attempted_)
            return;
        shinchan_textures_attempted_ = true;

        for (int i = 0; i < 12; ++i)
        {
            char path1[128];
            char path2[128];
            char path3[128];
            std::snprintf(path1, sizeof(path1), "/app0/assets/ui/shinchan_frames/frame_%02d.png", i + 1);
            std::snprintf(path2, sizeof(path2), "/data/homebrew/PPSA99001/assets/ui/shinchan_frames/frame_%02d.png", i + 1);
            std::snprintf(path3, sizeof(path3), "assets/ui/shinchan_frames/frame_%02d.png", i + 1);

            const char *candidates[] = { path1, path2, path3 };
            for (const char *path : candidates)
            {
                shinchan_dance_textures_[static_cast<std::size_t>(i)] = machete::scanner::load_texture_from_png(path);
                if (shinchan_dance_textures_[static_cast<std::size_t>(i)] != 0)
                    break;
            }
        }
    }

    const app::ConceptInfo &info() const override
    {
        static const app::ConceptInfo kInfo{
            "machete",
            "MacheteSVG Save Manager",
            "PS5 & PS4 Save Data Manager & Web Save Station",
            "src/concepts/machete.cpp",
            audio::SoundSet::glass,
            Color::rgb(0x76d6ff),
            kTechniques,
        };
        return kInfo;
    }

    void start_async_backup(const machete::scanner::ScannedGame &game,
                            const machete::storage::StorageDevice &dev)
    {
        if (op_thread_.joinable())
            op_thread_.join();

        is_operating_.store(true);
        operation_progress_.store(0.0f);
        operation_result_ready_.store(false);
        op_type_ = OperationType::Backup;

        // Activar animacion comica de Shin-chan durante la copia
        shinchan_state_ = ShinchanState::Dancing;
        shinchan_opacity_ = 1.0f;
        shinchan_timer_ = 0.0f;

        const bool is_es = (machete::i18n::current_language() == machete::i18n::Language::Spanish);
        {
            std::lock_guard<std::mutex> lock(op_mutex_);
            operation_title_ = is_es ? ("Respaldando " + (!game.title.empty() ? game.title : game.title_id))
                                     : ("Backing up " + (!game.title.empty() ? game.title : game.title_id));
            operation_detail_ = is_es ? ("Iniciando transferencia a " + dev.label + "...")
                                      : ("Starting transfer to " + dev.label + "...");
            operation_result_ = {};
        }

        MACHETE_LOG("[OPERATION]", "Starting background backup: %s -> %s",
                    game.title_id.c_str(), dev.label.c_str());

        op_thread_.start([this, game, dev]() {
            MACHETE_BREADCRUMB("op_thread: backup_game_save");
            const auto res = machete::save::backup_game_save(
                game, dev, [this](const machete::save::OperationProgress &p) {
                    operation_progress_.store(p.progress);
                    std::lock_guard<std::mutex> lock(op_mutex_);
                    char buf[256];
                    const double mb_done = static_cast<double>(p.bytes_transferred) / (1024.0 * 1024.0);
                    const double mb_tot = static_cast<double>(p.total_bytes) / (1024.0 * 1024.0);
                    if (mb_tot > 0.05)
                    {
                        std::snprintf(buf, sizeof(buf), "%.1f / %.1f MB  (%d%%)  -  %s",
                                      mb_done, mb_tot, static_cast<int>(p.progress * 100.0f),
                                      p.current_file.c_str());
                    }
                    else
                    {
                        std::snprintf(buf, sizeof(buf), "%d archivos  -  %s",
                                      p.files_transferred, p.current_file.c_str());
                    }
                    operation_detail_ = buf;
                });

            {
                std::lock_guard<std::mutex> lock(op_mutex_);
                operation_result_ = res;
            }
            MACHETE_LOG("[OPERATION]", "Background backup finished: success=%d, files=%d, bytes=%llu",
                        res.success ? 1 : 0, res.files_transferred,
                        static_cast<unsigned long long>(res.bytes_transferred));
            operation_progress_.store(1.0f);
            operation_result_ready_.store(true);
            is_operating_.store(false);
            MACHETE_BREADCRUMB("op_thread: backup_game_save done");
        });
    }

    void start_async_restore(const machete::scanner::ScannedGame &game,
                             const machete::save::BackupItem &backup)
    {
        if (op_thread_.joinable())
            op_thread_.join();

        is_operating_.store(true);
        operation_progress_.store(0.0f);
        operation_result_ready_.store(false);
        op_type_ = OperationType::Restore;

        // Activar animacion comica de Shin-chan durante la restauracion
        shinchan_state_ = ShinchanState::Dancing;
        shinchan_opacity_ = 1.0f;
        shinchan_timer_ = 0.0f;

        const bool is_es = (machete::i18n::current_language() == machete::i18n::Language::Spanish);
        {
            std::lock_guard<std::mutex> lock(op_mutex_);
            operation_title_ = is_es ? ("Restaurando " + (!game.title.empty() ? game.title : game.title_id))
                                     : ("Restoring " + (!game.title.empty() ? game.title : game.title_id));
            operation_detail_ = is_es ? "Extrayendo partida preservando Keystone..."
                                      : "Extracting save preserving Keystone...";
            operation_result_ = {};
        }

        MACHETE_LOG("[OPERATION]", "Starting background restore: %s <- %s",
                    game.title_id.c_str(), backup.folder_path.c_str());

        op_thread_.start([this, game, backup]() {
            MACHETE_BREADCRUMB("op_thread: restore_backup_item");
            const auto res = machete::save::restore_backup_item(
                game, backup, [this](const machete::save::OperationProgress &p) {
                    operation_progress_.store(p.progress);
                    std::lock_guard<std::mutex> lock(op_mutex_);
                    char buf[256];
                    const double mb_done = static_cast<double>(p.bytes_transferred) / (1024.0 * 1024.0);
                    const double mb_tot = static_cast<double>(p.total_bytes) / (1024.0 * 1024.0);
                    if (mb_tot > 0.05)
                    {
                        std::snprintf(buf, sizeof(buf), "%.1f / %.1f MB  (%d%%)  -  %s",
                                      mb_done, mb_tot, static_cast<int>(p.progress * 100.0f),
                                      p.current_file.c_str());
                    }
                    else
                    {
                        std::snprintf(buf, sizeof(buf), "%d archivos  -  %s",
                                      p.files_transferred, p.current_file.c_str());
                    }
                    operation_detail_ = buf;
                });

            {
                std::lock_guard<std::mutex> lock(op_mutex_);
                operation_result_ = res;
            }
            MACHETE_LOG("[OPERATION]", "Background restore finished: success=%d, files=%d, bytes=%llu",
                        res.success ? 1 : 0, res.files_transferred,
                        static_cast<unsigned long long>(res.bytes_transferred));
            operation_progress_.store(1.0f);
            operation_result_ready_.store(true);
            is_operating_.store(false);
            MACHETE_BREADCRUMB("op_thread: restore_backup_item done");
        });
    }

    std::vector<int> get_filtered_game_indices() const
    {
        std::vector<int> indices;
        indices.reserve(games_.size());
        for (int i = 0; i < static_cast<int>(games_.size()); ++i)
        {
            const auto &g = games_[static_cast<std::size_t>(i)];
            if (current_tab_ == GameTab::PS5 && g.platform != "PS5")
                continue;
            if (current_tab_ == GameTab::PS4 && g.platform != "PS4")
                continue;
            indices.push_back(i);
        }
        return indices;
    }

    void get_tab_counts(int &ps5_count, int &ps4_count, int &all_count) const
    {
        ps5_count = 0;
        ps4_count = 0;
        all_count = static_cast<int>(games_.size());
        for (const auto &g : games_)
        {
            if (g.platform == "PS5")
                ps5_count++;
            else if (g.platform == "PS4")
                ps4_count++;
        }
    }

    const machete::scanner::ScannedGame *current_selected_game() const
    {
        const auto filtered = get_filtered_game_indices();
        if (filtered.empty() || selected_game_ < 0 || selected_game_ >= static_cast<int>(filtered.size()))
            return nullptr;
        return &games_[static_cast<std::size_t>(filtered[static_cast<std::size_t>(selected_game_)])];
    }

    void switch_tab(GameTab tab, app::Feedback &feedback)
    {
        if (current_tab_ == tab)
            return;
        current_tab_ = tab;
        selected_game_ = 0;
        scroll_offset_ = 0;
        tab_indicator_.target = static_cast<float>(current_tab_);
        feedback.play(audio::Cue::tab, 1.0f, 0.0f);

        const auto filtered = get_filtered_game_indices();
        if (focus_ >= kGame0 && focus_ <= kGame4)
        {
            if (filtered.empty())
            {
                const Item target_tab = (tab == GameTab::PS5) ? kTabPS5 :
                                        ((tab == GameTab::PS4) ? kTabPS4 : kTabAll);
                move_to(target_tab, feedback);
            }
            else
            {
                move_to(kGame0, feedback);
            }
        }
    }

    void start_async_jailbreak()
    {
        MACHETE_BREADCRUMB("start_async_jailbreak requested");
        if (jb_thread_running_.exchange(true))
            return;

        if (jb_thread_.joinable())
            jb_thread_.join();

        jb_thread_.start([this]() {
            MACHETE_BREADCRUMB("jb_thread running request_jailbreak");
            const auto result = machete::jailbreak::request_jailbreak();
            {
                std::lock_guard<std::mutex> lock(jb_mutex_);
                jb_info_ = result;
            }
            if (result.root_access)
            {
                MACHETE_BREADCRUMB("jb_thread: root access granted");
                root_active_.store(true);
                jb_needs_refresh_.store(true);
            }
            else
            {
                MACHETE_BREADCRUMB("jb_thread: elevation not completed yet");
            }
            jb_thread_running_.store(false);
        });
    }

    void enter() override
    {
        MACHETE_BREADCRUMB("MacheteConcept::enter start");
        age_ = 0.0f;
        dialog_ = false;
        web_server_dialog_ = false;
        alert_dialog_ = false;
        notification_timer_ = 0.0f;
        storage_poll_timer_ = 0.0f;
        jb_poll_timer_ = 0.0f;

        // 1. Verificación instantánea sin bloqueo (< 1ms)
        MACHETE_BREADCRUMB("MacheteConcept::enter: check_status");
        jb_info_ = machete::jailbreak::check_status();
        root_active_.store(jb_info_.root_access);
        if (jb_info_.root_access)
        {
            MACHETE_BREADCRUMB("MacheteConcept::enter: cleaning shadowmount");
            machete::jailbreak::clean_shadowmount_interference();
        }

        // 2. Escaneo de dispositivos de almacenamiento
        MACHETE_BREADCRUMB("MacheteConcept::enter: scan_storage_devices");
        active_devices_ = machete::storage::scan_storage_devices();

        // 3. Escaneo dinámico de juegos y partidas
        MACHETE_BREADCRUMB("MacheteConcept::enter: initial refresh_games");
        refresh_games();

        // 4. Si aún no tenemos privilegios root, lanzar elevación en segundo plano
        if (!root_active_.load())
        {
            MACHETE_BREADCRUMB("MacheteConcept::enter: launching async jailbreak");
            start_async_jailbreak();
        }

        // 5. Cargar emblema oficial de la aplicacion
        load_app_icon();

        MACHETE_BREADCRUMB("MacheteConcept::enter finished");
    }

    void refresh_games()
    {
        MACHETE_BREADCRUMB("refresh_games: start");
        machete::scanner::release_game_icon_textures(games_);
        MACHETE_BREADCRUMB("refresh_games: scan_installed_games_and_saves");
        games_ = machete::scanner::scan_installed_games_and_saves();
        MACHETE_BREADCRUMB("refresh_games: load_game_icon_textures");
        machete::scanner::load_game_icon_textures(games_);
        machete::server::WebServer::instance().update_games(games_);

        if (games_.empty())
        {
            selected_game_ = 0;
            scroll_offset_ = 0;
        }
        else
        {
            const auto filtered = get_filtered_game_indices();
            if (filtered.empty())
            {
                selected_game_ = 0;
                scroll_offset_ = 0;
            }
            else
            {
                selected_game_ = std::clamp(selected_game_, 0, static_cast<int>(filtered.size()) - 1);
                adjust_scroll();
            }
        }
        MACHETE_LOG("[SCANNER]", "scan completed with %zu games detected", games_.size());
        MACHETE_BREADCRUMB("refresh_games: completed");
    }

    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        age_ += dt;
        clock_ += dt;
        if (notification_timer_ > 0.0f)
            notification_timer_ -= dt;

        // Sondeo periódico en segundo plano de inserción/extracción USB
        storage_poll_timer_ += dt;
        if (storage_poll_timer_ >= 5.0f)
        {
            storage_poll_timer_ = 0.0f;
            std::string hotplug_msg;
            MACHETE_BREADCRUMB("machete: poll_storage_changes");
            if (machete::storage::poll_storage_changes(active_devices_, hotplug_msg))
            {
                std::snprintf(notification_text_, sizeof(notification_text_), "%s",
                              hotplug_msg.c_str());
                notification_timer_ = 4.0f;
                feedback.play(audio::Cue::connect, 1.0f, 0.0f);
            }
        }

        // Notificación de elevación de privilegios completada en segundo plano
        if (jb_needs_refresh_.exchange(false))
        {
            MACHETE_BREADCRUMB("machete: jb_needs_refresh");
            machete::jailbreak::clean_shadowmount_interference();
            machete::storage::ensure_internal_storage();
            active_devices_ = machete::storage::scan_storage_devices();
            refresh_games();
            const bool is_es = (machete::i18n::current_language() == machete::i18n::Language::Spanish);
            std::snprintf(notification_text_, sizeof(notification_text_), "%s",
                          is_es ? "Jailbreak Concedido Exitosamente"
                                : "Jailbreak Granted Successfully");
            notification_timer_ = 4.0f;
            feedback.play(audio::Cue::welcome, 1.0f, 0.0f);
        }

        // Sondeo periódico no bloqueante de privilegios (solo si aún no somos root)
        if (!root_active_.load())
        {
            jb_poll_timer_ += dt;
            if (jb_poll_timer_ >= 3.0f)
            {
                jb_poll_timer_ = 0.0f;
                if (!jb_thread_running_.load())
                {
                    MACHETE_BREADCRUMB("machete: check_status poll");
                    const auto current = machete::jailbreak::check_status();
                    if (current.root_access)
                    {
                        {
                            std::lock_guard<std::mutex> lock(jb_mutex_);
                            jb_info_ = current;
                        }
                        root_active_.store(true);
                        machete::storage::ensure_internal_storage();
                        active_devices_ = machete::storage::scan_storage_devices();
                        refresh_games();
                        notification_timer_ = 4.0f;
                        std::snprintf(notification_text_, sizeof(notification_text_), "%s",
                                      machete::i18n::tr(machete::i18n::StringId::ToastJailbreakGranted));
                        feedback.play(audio::Cue::welcome, 1.0f, 0.0f);
                    }
                }
            }
        }

        // Verificación de finalización de operación asíncrona en segundo plano
        if (operation_result_ready_.exchange(false))
        {
            machete::save::OperationResult res;
            {
                std::lock_guard<std::mutex> lock(op_mutex_);
                res = operation_result_;
            }
            if (op_type_ == OperationType::Restore && res.success)
            {
                refresh_games();
            }

            // Actualización del estado del personaje animado (Shin-chan festeja o llora)
            if (res.success)
            {
                shinchan_state_ = ShinchanState::Success;
            }
            else
            {
                shinchan_state_ = ShinchanState::Error;
            }
            shinchan_opacity_ = 1.0f;
            shinchan_timer_ = 6.0f; // 6 segundos de celebracion / aviso comico

            const bool is_es = (machete::i18n::current_language() == machete::i18n::Language::Spanish);
            if (!res.success || res.message.size() > 45)
            {
                const std::string title = !res.success
                    ? (is_es ? "Aviso de Operacion" : "Operation Notice")
                    : (is_es ? "Operacion Completada" : "Operation Completed");
                notification_timer_ = 0.0f;
                notification_text_[0] = '\0';
                open_alert_dialog(!res.success, title, res.message, feedback);
            }
            else
            {
                notification_timer_ = 4.0f;
                std::snprintf(notification_text_, sizeof(notification_text_), "%s", res.message.c_str());
                feedback.play(res.success ? audio::Cue::saved : audio::Cue::error, 1.0f, 0.0f);
            }

            op_type_ = OperationType::None;
        }

        const bool operating = is_operating_.load();
        op_overlay_show_.target = operating ? 1.0f : 0.0f;
        op_overlay_show_.update(dt, 22.0f);
        op_progress_bar_.target = operation_progress_.load();
        op_progress_bar_.update(dt, 16.0f);
        if (operating || op_overlay_show_.value > 0.01f)
        {
            op_spinner_angle_ += dt * 360.0f * 1.5f;
            if (op_spinner_angle_ >= 360.0f)
                op_spinner_angle_ -= 360.0f;
        }

        // Actualización continua de Shin-chan y temporizador suave de desvanecimiento (fade-out)
        if (operating)
        {
            shinchan_state_ = ShinchanState::Dancing;
            shinchan_timer_ = 0.0f;
            shinchan_opacity_ = std::min(1.0f, shinchan_opacity_ + dt * 4.0f);
        }
        else if (shinchan_state_ == ShinchanState::Success || shinchan_state_ == ShinchanState::Error)
        {
            if (shinchan_timer_ > 0.0f)
            {
                shinchan_timer_ -= dt;
                if (shinchan_timer_ <= 1.5f)
                {
                    shinchan_opacity_ = std::max(0.0f, shinchan_timer_ / 1.5f);
                }
                if (shinchan_timer_ <= 0.0f)
                {
                    shinchan_state_ = ShinchanState::Hidden;
                    shinchan_opacity_ = 0.0f;
                }
            }
        }

        // Actualizacion fisica del indicador de pestaña activa
        tab_indicator_.target = static_cast<float>(current_tab_);
        tab_indicator_.update(dt, 26.0f);

        const ui::Theme &theme = ui::themes()[static_cast<std::size_t>(theme_)];
        const int count = static_cast<int>(ui::themes().size());

        // L1 / R1: Conmutar pestañas de plataforma (PS5 / PS4 / TODOS)
        if (input.is_pressed(Action::page_prev))
        {
            const int next_tab = (static_cast<int>(current_tab_) + 2) % 3;
            switch_tab(static_cast<GameTab>(next_tab), feedback);
        }
        else if (input.is_pressed(Action::page_next))
        {
            const int next_tab = (static_cast<int>(current_tab_) + 1) % 3;
            switch_tab(static_cast<GameTab>(next_tab), feedback);
        }
        // L2 / R2: Cambio de tema en tiempo real
        else if (input.is_pressed(Action::jump_next) || input.is_pressed(Action::jump_prev))
        {
            previous_theme_ = theme_;
            theme_ = (theme_ + (input.is_pressed(Action::jump_next) ? 1 : count - 1)) % count;
            switch_.start(context_.settings.reduced_motion ? 0.16f : 0.42f);
            feedback.play(audio::Cue::tab, 1.0f, 0.0f);
        }
        // Protección contra spam de botones mientras se realiza una operación en segundo plano
        else if (operating || op_overlay_show_.value > 0.05f)
        {
            if (input.is_pressed(Action::confirm) || input.is_pressed(Action::west) ||
                input.is_pressed(Action::north) || input.is_pressed(Action::back) ||
                input.nav != Direction::none)
            {
                refuse_.trigger();
                feedback.play(audio::Cue::error, 1.0f, 0.0f);
            }
        }
        else if (alert_dialog_)
        {
            update_alert_dialog(input, feedback);
        }
        // Atajo Triangle: Abrir / alternar servidor web local directamente
        else if (input.is_pressed(Action::north))
        {
            if (web_server_dialog_)
            {
                web_server_dialog_ = false;
                feedback.play(audio::Cue::modal_close, 1.0f, 0.0f);
            }
            else
            {
                open_web_server_dialog(feedback);
            }
        }
        // Atajo Square: Recargar lista de juegos y reintentar elevación si es necesario
        else if (input.is_pressed(Action::west))
        {
            if (!root_active_.load())
                start_async_jailbreak();
            machete::storage::ensure_internal_storage();
            active_devices_ = machete::storage::scan_storage_devices();
            refresh_games();
            std::snprintf(notification_text_, sizeof(notification_text_),
                          machete::i18n::tr(machete::i18n::StringId::ToastScanUpdated), games_.size());
            notification_timer_ = 3.0f;
            feedback.play(audio::Cue::select, 1.0f, 0.0f);
        }
        // Atajo L3: Alternar Idioma (Inglés / Español)
        else if (input.is_pressed(Action::l3))
        {
            machete::i18n::toggle_language();
            std::snprintf(notification_text_, sizeof(notification_text_), "%s",
                          machete::i18n::tr(machete::i18n::StringId::ToastLanguageSwitched));
            notification_timer_ = 2.5f;
            feedback.play(audio::Cue::select, 1.0f, 0.0f);
        }
        else if (web_server_dialog_)
        {
            update_web_server_dialog(input, feedback);
        }
        else if (dialog_)
        {
            update_dialog(input, feedback);
        }
        else
        {
            update_navigation(input, feedback);
            if (input.is_pressed(Action::confirm))
                activate(feedback);
        }

        // Animaciones e inercia física (Spring physics)
        const float omega = context_.settings.reduced_motion ? 60.0f : theme.omega;
        switch_.update(dt);

        for (int i = 0; i < kItemCount; ++i)
        {
            const bool focused = (!alert_dialog_ && !dialog_ && !web_server_dialog_ && i == focus_ && !operating &&
                                  op_overlay_show_.value <= 0.05f);
            focus_amount_[i].target = focused ? 1.0f : 0.0f;
            focus_amount_[i].update(dt, std::max(omega, 22.0f));
            press_[i].update(dt, 9.0f);
        }

        dialog_show_.target = dialog_ ? 1.0f : 0.0f;
        dialog_show_.update(dt, std::min(omega, 26.0f));

        web_server_dialog_show_.target = web_server_dialog_ ? 1.0f : 0.0f;
        web_server_dialog_show_.update(dt, std::min(omega, 26.0f));

        alert_dialog_show_.target = alert_dialog_ ? 1.0f : 0.0f;
        alert_dialog_show_.update(dt, std::min(omega, 26.0f));

        dialog_focus_value_.target = static_cast<float>(dialog_focus_);
        dialog_focus_value_.update(dt, std::max(omega, 20.0f));
        refuse_.update(dt, 9.0f);
    }

    void draw(app::Frame &frame) const override
    {
        const auto themes = ui::themes();
        const ui::Theme &now = themes[static_cast<std::size_t>(theme_)];
        const ui::Theme &old = themes[static_cast<std::size_t>(previous_theme_)];
        const float t = switch_.running ? switch_.progress() : 1.0f;

        const ui::Theme &behind = t < 0.5f ? old : now;
        frame.backdrop = behind.backdrop;
        frame.backdrop.time = clock_;

        if (behind.style == ui::SurfaceStyle::glass)
        {
            for (int i = 0; i < 4; ++i)
            {
                const float phase = clock_ * 0.12f + static_cast<float>(i) * 1.7f;
                frame.scene.circle(
                    480.0f + static_cast<float>(i) * 380.0f + std::sin(phase) * 120.0f,
                    560.0f + std::cos(phase * 1.3f) * 220.0f, 170.0f,
                    gfx::mix(behind.primary, behind.accent, static_cast<float>(i % 2))
                        .with_alpha(0.55f));
            }
        }

        draw_dashboard(frame.scene, now, frame.glass_texture, 1.0f, 0.0f);
    }

  private:
    void adjust_scroll()
    {
        const auto filtered = get_filtered_game_indices();
        const int total = static_cast<int>(filtered.size());
        if (total <= 5)
        {
            scroll_offset_ = 0;
            return;
        }
        if (selected_game_ < scroll_offset_)
            scroll_offset_ = selected_game_;
        else if (selected_game_ >= scroll_offset_ + 5)
            scroll_offset_ = selected_game_ - 4;
        scroll_offset_ = std::clamp(scroll_offset_, 0, total - 5);
    }

    void update_navigation(const InputFrame &input, app::Feedback &feedback)
    {
        const auto filtered = get_filtered_game_indices();
        const int total_games = static_cast<int>(filtered.size());

        // Navegación en las Pestañas Superiores (PS5, PS4, TODOS)
        if (focus_ >= kTabPS5 && focus_ <= kTabAll)
        {
            if (input.nav == Direction::right)
            {
                if (focus_ == kTabPS5)
                    move_to(kTabPS4, feedback);
                else if (focus_ == kTabPS4)
                    move_to(kTabAll, feedback);
                else
                    move_to(kBtnBackup, feedback);
            }
            else if (input.nav == Direction::left)
            {
                if (focus_ == kTabAll)
                    move_to(kTabPS4, feedback);
                else if (focus_ == kTabPS4)
                    move_to(kTabPS5, feedback);
                else
                    refuse(feedback, input);
            }
            else if (input.nav == Direction::down)
            {
                if (total_games > 0)
                {
                    const int visible_idx = std::clamp(selected_game_ - scroll_offset_, 0, std::min(4, total_games - 1));
                    move_to(kGame0 + visible_idx, feedback);
                }
                else
                {
                    move_to(kBtnBackup, feedback);
                }
            }
            else if (input.nav == Direction::up)
            {
                move_to(kBtnWebServerHeader, feedback);
            }
            else
            {
                refuse(feedback, input);
            }
        }
        // Navegación en la Lista de Juegos
        else if (focus_ >= kGame0 && focus_ <= kGame4)
        {
            if (input.nav == Direction::down)
            {
                if (selected_game_ < total_games - 1)
                {
                    selected_game_++;
                    adjust_scroll();
                    const int visible_idx = selected_game_ - scroll_offset_;
                    move_to(kGame0 + visible_idx, feedback);
                }
                else
                {
                    refuse(feedback, input);
                }
            }
            else if (input.nav == Direction::up)
            {
                if (selected_game_ > 0)
                {
                    selected_game_--;
                    adjust_scroll();
                    const int visible_idx = selected_game_ - scroll_offset_;
                    move_to(kGame0 + visible_idx, feedback);
                }
                else
                {
                    // Subir a la pestaña actualmente seleccionada
                    const Item target_tab = (current_tab_ == GameTab::PS5) ? kTabPS5 :
                                            ((current_tab_ == GameTab::PS4) ? kTabPS4 : kTabAll);
                    move_to(target_tab, feedback);
                }
            }
            else if (input.nav == Direction::right)
            {
                move_to(kBtnBackup, feedback);
            }
            else
            {
                refuse(feedback, input);
            }
        }
        // Navegación en la Cabecera (Chip Servidor Web)
        else if (focus_ == kBtnWebServerHeader)
        {
            if (input.nav == Direction::down)
            {
                move_to(kBtnBackup, feedback);
            }
            else if (input.nav == Direction::left)
            {
                const Item target_tab = (current_tab_ == GameTab::PS5) ? kTabPS5 :
                                        ((current_tab_ == GameTab::PS4) ? kTabPS4 : kTabAll);
                move_to(target_tab, feedback);
            }
            else
            {
                refuse(feedback, input);
            }
        }
        // Navegación en el Panel Derecho (Botones de Acción)
        else
        {
            if (input.nav == Direction::left)
            {
                if (total_games == 0)
                {
                    const Item target_tab = (current_tab_ == GameTab::PS5) ? kTabPS5 :
                                            ((current_tab_ == GameTab::PS4) ? kTabPS4 : kTabAll);
                    move_to(target_tab, feedback);
                }
                else
                {
                    const int visible_idx = std::clamp(selected_game_ - scroll_offset_, 0, std::min(4, total_games - 1));
                    move_to(kGame0 + visible_idx, feedback);
                }
            }
            else if (focus_ == kBtnBackup)
            {
                if (input.nav == Direction::right)
                    move_to(kBtnRestore, feedback);
                else if (input.nav == Direction::down)
                    move_to(kBtnWebServer, feedback);
                else if (input.nav == Direction::up)
                    move_to(kBtnWebServerHeader, feedback);
                else
                    refuse(feedback, input);
            }
            else if (focus_ == kBtnRestore)
            {
                if (input.nav == Direction::left)
                    move_to(kBtnBackup, feedback);
                else if (input.nav == Direction::down)
                    move_to(kBtnWebServer, feedback);
                else if (input.nav == Direction::up)
                    move_to(kBtnWebServerHeader, feedback);
                else
                    refuse(feedback, input);
            }
            else if (focus_ == kBtnWebServer)
            {
                if (input.nav == Direction::up)
                    move_to(kBtnBackup, feedback);
                else
                    refuse(feedback, input);
            }
        }
    }

    void move_to(int item, app::Feedback &feedback)
    {
        if (item == focus_)
            return;
        focus_ = item;
        feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(kRects[item].cx()));
    }

    void refuse(app::Feedback &feedback, const InputFrame &input)
    {
        if (input.nav != Direction::none)
        {
            refuse_.trigger();
            feedback.play(audio::Cue::error, 1.0f, ui::pan_for_x(kRects[focus_].cx()));
        }
    }

    void activate(app::Feedback &feedback)
    {
        if (is_operating_.load() || op_overlay_show_.value > 0.05f)
        {
            feedback.play(audio::Cue::error, 1.0f, 0.0f);
            return;
        }

        press_[focus_].trigger();

        if (focus_ == kTabPS5)
        {
            switch_tab(GameTab::PS5, feedback);
        }
        else if (focus_ == kTabPS4)
        {
            switch_tab(GameTab::PS4, feedback);
        }
        else if (focus_ == kTabAll)
        {
            switch_tab(GameTab::All, feedback);
        }
        else if (focus_ >= kGame0 && focus_ <= kGame4)
        {
            feedback.play(audio::Cue::select, 1.0f, ui::pan_for_x(kRects[focus_].cx()));
        }
        else if (focus_ == kBtnBackup)
        {
            const auto *game = current_selected_game();
            if (!game)
                return;
            machete::storage::ensure_internal_storage();
            active_devices_ = machete::storage::scan_storage_devices();
            dialog_is_backup_ = true;
            dialog_ = true;
            dialog_focus_ = 0;
            feedback.play(audio::Cue::modal_open, 1.0f, ui::pan_for_x(kRects[focus_].cx()));
        }
        else if (focus_ == kBtnRestore)
        {
            const auto *game = current_selected_game();
            if (!game)
                return;
            machete::storage::ensure_internal_storage();
            active_devices_ = machete::storage::scan_storage_devices();
            MACHETE_BREADCRUMB("machete: find_available_backups");
            available_backups_ = machete::save::find_available_backups(*game, active_devices_);
            dialog_is_backup_ = false;
            dialog_ = true;
            dialog_focus_ = 0;
            feedback.play(audio::Cue::modal_open, 1.0f, ui::pan_for_x(kRects[focus_].cx()));
        }
        else if (focus_ == kBtnWebServer || focus_ == kBtnWebServerHeader)
        {
            open_web_server_dialog(feedback);
        }
    }

    static std::vector<std::string> wrap_text_into_lines(std::string_view text, std::size_t max_chars = 62)
    {
        std::vector<std::string> lines;
        if (text.empty())
            return lines;

        std::size_t pos = 0;
        while (pos < text.size())
        {
            const std::size_t next_nl = text.find('\n', pos);
            const std::string_view segment = (next_nl == std::string_view::npos)
                                                 ? text.substr(pos)
                                                 : text.substr(pos, next_nl - pos);

            if (segment.empty())
            {
                lines.emplace_back("");
            }
            else
            {
                std::size_t start = 0;
                while (start < segment.size())
                {
                    if (start + max_chars >= segment.size())
                    {
                        lines.emplace_back(std::string(segment.substr(start)));
                        break;
                    }
                    const std::size_t end = start + max_chars;
                    const std::size_t last_space = segment.rfind(' ', end);
                    if (last_space != std::string_view::npos && last_space > start)
                    {
                        lines.emplace_back(std::string(segment.substr(start, last_space - start)));
                        start = last_space + 1;
                    }
                    else
                    {
                        lines.emplace_back(std::string(segment.substr(start, max_chars)));
                        start += max_chars;
                    }
                }
            }

            if (next_nl == std::string_view::npos)
                break;
            pos = next_nl + 1;
        }
        return lines;
    }

    void open_alert_dialog(bool is_error, const std::string &title, const std::string &message,
                           app::Feedback &feedback)
    {
        alert_is_error_ = is_error;
        alert_title_ = title;
        alert_message_ = message;
        alert_lines_ = wrap_text_into_lines(message, 62);
        alert_dialog_ = true;
        notification_timer_ = 0.0f;
        notification_text_[0] = '\0';
        feedback.play(is_error ? audio::Cue::error : audio::Cue::saved, 1.0f, 0.0f);
    }

    void update_alert_dialog(const InputFrame &input, app::Feedback &feedback)
    {
        if (input.is_pressed(Action::confirm) || input.is_pressed(Action::back))
        {
            alert_dialog_ = false;
            feedback.play(audio::Cue::modal_close, 1.0f, 0.0f);
        }
    }

    void open_web_server_dialog(app::Feedback &feedback)
    {
        web_server_dialog_ = true;
        web_server_dialog_focus_ = 0;
        feedback.play(audio::Cue::modal_open, 1.0f, 0.0f);
    }

    void update_web_server_dialog(const InputFrame &input, app::Feedback &feedback)
    {
        auto &server = machete::server::WebServer::instance();
        const bool is_running = server.is_running();
        const int max_focus = 1;

        if (input.nav == Direction::down || input.nav == Direction::right)
        {
            if (web_server_dialog_focus_ < max_focus)
            {
                web_server_dialog_focus_++;
                feedback.play(audio::Cue::focus, 1.0f, 0.0f);
            }
        }
        else if (input.nav == Direction::up || input.nav == Direction::left)
        {
            if (web_server_dialog_focus_ > 0)
            {
                web_server_dialog_focus_--;
                feedback.play(audio::Cue::focus, 1.0f, 0.0f);
            }
        }

        if (input.is_pressed(Action::back))
        {
            web_server_dialog_ = false;
            feedback.play(audio::Cue::modal_close, 1.0f, 0.0f);
        }
        else if (input.is_pressed(Action::confirm))
        {
            if (web_server_dialog_focus_ == 0)
            {
                if (is_running)
                {
                    server.stop();
                    std::snprintf(notification_text_, sizeof(notification_text_), "%s",
                                  machete::i18n::tr(machete::i18n::StringId::ToastWebServerStopped));
                    notification_timer_ = 3.5f;
                    feedback.play(audio::Cue::modal_close, 1.0f, 0.0f);
                }
                else
                {
                    server.update_games(games_);
                    const bool ok = server.start(machete::server::kDefaultPort);
                    if (ok)
                    {
                        std::snprintf(notification_text_, sizeof(notification_text_), "%s",
                                      machete::i18n::tr(machete::i18n::StringId::ToastWebServerStarted));
                        notification_timer_ = 4.0f;
                        feedback.play(audio::Cue::saved, 1.0f, 0.0f);
                    }
                    else
                    {
                        const bool is_es = (machete::i18n::current_language() == machete::i18n::Language::Spanish);
                        std::snprintf(notification_text_, sizeof(notification_text_),
                                      is_es ? "Error al iniciar servidor en puerto %u"
                                            : "Failed to start server on port %u",
                                      static_cast<unsigned>(machete::server::kDefaultPort));
                        notification_timer_ = 4.0f;
                        feedback.play(audio::Cue::error, 1.0f, 0.0f);
                    }
                }
            }
            else
            {
                web_server_dialog_ = false;
                feedback.play(audio::Cue::modal_close, 1.0f, 0.0f);
            }
        }
    }

    void update_dialog(const InputFrame &input, app::Feedback &feedback)
    {
        if (dialog_is_backup_)
        {
            const int opt_count = static_cast<int>(active_devices_.size());
        if (input.nav == Direction::down)
        {
            if (dialog_focus_ < opt_count)
            {
                dialog_focus_++;
                feedback.play(audio::Cue::focus, 1.0f, 0.0f);
            }
        }
        else if (input.nav == Direction::up)
        {
            if (dialog_focus_ > 0)
            {
                dialog_focus_--;
                feedback.play(audio::Cue::focus, 1.0f, 0.0f);
            }
        }

        if (input.is_pressed(Action::back))
        {
            dialog_ = false;
            feedback.play(audio::Cue::modal_close, 1.0f, 0.0f);
        }
        else if (input.is_pressed(Action::confirm))
        {
            if (dialog_focus_ == opt_count) // Botón Cancelar
            {
                dialog_ = false;
                feedback.play(audio::Cue::modal_close, 1.0f, 0.0f);
            }
            else if (dialog_focus_ >= 0 && dialog_focus_ < opt_count)
            {
                const auto *game_ptr = current_selected_game();
                if (!game_ptr)
                    return;
                const auto &game = *game_ptr;
                auto &dev = active_devices_[static_cast<std::size_t>(dialog_focus_)];

                if (is_operating_.load())
                {
                    feedback.play(audio::Cue::error, 1.0f, 0.0f);
                    return;
                }

                if (dev.type == machete::storage::StorageType::Internal)
                {
                    machete::storage::ensure_internal_storage();
                    if (!dev.connected && (access("/data", F_OK) == 0 || access(dev.root_dir.c_str(), F_OK) == 0))
                    {
                        dev.connected = true;
                    }
                }

                dialog_ = false;
                feedback.play(audio::Cue::select, 1.0f, 0.0f);
                start_async_backup(game, dev);
            }
        }
    }
    else
    {
        // DIALOGO DE RESTAURACION: SELECCION DE COPIA ESPECIFICA
        if (available_backups_.empty())
        {
            if (input.is_pressed(Action::back) || input.is_pressed(Action::confirm))
            {
                dialog_ = false;
                feedback.play(audio::Cue::modal_close, 1.0f, 0.0f);
            }
        }
        else
        {
            const int opt_count = static_cast<int>(available_backups_.size());
            if (input.nav == Direction::down)
            {
                if (dialog_focus_ < opt_count)
                {
                    dialog_focus_++;
                    feedback.play(audio::Cue::focus, 1.0f, 0.0f);
                }
            }
            else if (input.nav == Direction::up)
            {
                if (dialog_focus_ > 0)
                {
                    dialog_focus_--;
                    feedback.play(audio::Cue::focus, 1.0f, 0.0f);
                }
            }

            if (input.is_pressed(Action::back))
            {
                dialog_ = false;
                feedback.play(audio::Cue::modal_close, 1.0f, 0.0f);
            }
            else if (input.is_pressed(Action::confirm))
            {
                if (dialog_focus_ == opt_count) // Boton Cancelar
                {
                    dialog_ = false;
                    feedback.play(audio::Cue::modal_close, 1.0f, 0.0f);
                }
                else if (dialog_focus_ >= 0 && dialog_focus_ < opt_count)
                {
                    if (is_operating_.load())
                    {
                        feedback.play(audio::Cue::error, 1.0f, 0.0f);
                        return;
                    }

                    const auto *game_ptr = current_selected_game();
                    if (!game_ptr)
                        return;
                    const auto &game = *game_ptr;
                    const auto &backup = available_backups_[static_cast<std::size_t>(dialog_focus_)];

                    dialog_ = false;
                    feedback.play(audio::Cue::select, 1.0f, 0.0f);
                    start_async_restore(game, backup);
                }
            }
        }
    }
}

    ui::Look look(int item) const
    {
        return {focus_amount_[item].value, press_[item].value, false};
    }

    void draw_dashboard(gfx::DrawList &list, const ui::Theme &theme, std::uint32_t glass,
                        float alpha, float slide) const
    {
        if (alpha <= 0.01f)
            return;
        const ui::Fonts &fonts = context_.fonts;
        ui::Painter paint(list, fonts, theme, glass);
        const float in = tween::cubic_out(age_ / 0.45f);

        list.push_opacity(alpha * in);
        list.push_transform(1.0f, 0, 0, 0, slide + 24.0f * (1.0f - in));

        // ==========================================
        // 1. CABECERA Y LOGO EMBLEMA (Apple Liquid Glass)
        // ==========================================
        const Rect logo_rect{96, 92, 60, 60};
        if (app_icon_texture_ == 0)
        {
            load_app_icon();
        }

        if (app_icon_texture_ != 0)
        {
            // Logotipo oficial de MacheteSVG (icon0.png) con esquinas redondeadas
            list.image(app_icon_texture_, logo_rect, gfx::kFullUv, Color::rgb(0xffffff), 14.0f);
            paint.stroke(logo_rect, 14.0f, 1.5f, Color::rgb(0x76d6ff).with_alpha(0.35f));
        }
        else
        {
            // Fallback en caso de que icon0.png no este disponible
            list.rounded_rect(logo_rect, 14.0f, Color::rgb(0x101b3d).with_alpha(0.85f));
            paint.stroke(logo_rect, 14.0f, 1.5f, Color::rgb(0x76d6ff).with_alpha(0.35f));
            list.rounded_rect({logo_rect.x + 16, logo_rect.y + 14, 28, 34}, 4.0f,
                              Color::rgb(0x76d6ff).with_alpha(0.25f));
            list.line(logo_rect.x + 12, logo_rect.y + 46, logo_rect.x + 48, logo_rect.y + 14, 3.0f,
                      Color::rgb(0x76d6ff));
        }

        paint.heading(machete::i18n::tr(machete::i18n::StringId::AppTitle), 172, 138, 44, paint.page_text());
        paint.label(machete::i18n::tr(machete::i18n::StringId::AppSubtitle), 174, 166, 17, paint.page_text_muted());

        // Badge de Versión y Build (Estilo Cyber-Pill Liquid Glass)
        char ver_str[64];
        std::snprintf(ver_str, sizeof(ver_str), "v%s (Build #%s)",
                      machete::build::kAppVersion, machete::build::kBuildNumber);
        const Rect ver_chip{940, 108, 226, 44};
        list.rounded_rect(ver_chip, 22.0f, Color::rgb(0x132242).with_alpha(0.8f));
        paint.stroke(ver_chip, 22.0f, 1.5f, Color::rgb(0x76d6ff).with_alpha(0.35f));
        list.circle(ver_chip.x + 22, ver_chip.cy(), 5.0f, Color::rgb(0x76d6ff));
        paint.body(ver_str, ver_chip.x + 36, ver_chip.cy() + 7, 17, Color::rgb(0x76d6ff));

        // Badge de Estado del Jailbreak / Daemon
        const Rect jb_chip{1190, 108, 330, 44};
        const bool is_root = root_active_.load();
        list.rounded_rect(jb_chip, 22.0f, Color::rgb(0x132242).with_alpha(0.8f));
        paint.stroke(jb_chip, 22.0f, 1.5f,
                     is_root ? Color::rgb(0x38ef7d).with_alpha(0.4f)
                             : Color::rgb(0xffaa00).with_alpha(0.4f));
        list.circle(jb_chip.x + 22, jb_chip.cy(), 5.0f,
                    is_root ? Color::rgb(0x38ef7d) : Color::rgb(0xffaa00));
        paint.body(is_root ? machete::i18n::tr(machete::i18n::StringId::JailbreakActiveRoot)
                           : machete::i18n::tr(machete::i18n::StringId::SandboxRestricted),
                   jb_chip.x + 36, jb_chip.cy() + 7, 18, Color::rgb(0xffffff));

        // Botón / Chip de Servidor Web (Navegable y Enfocable)
        const Rect web_chip = kRects[kBtnWebServerHeader];
        const bool web_focused = (focus_ == kBtnWebServerHeader);
        const bool server_running = machete::server::WebServer::instance().is_running();

        list.rounded_rect(web_chip, 22.0f,
                          web_focused ? Color::rgb(0x1d3b66).with_alpha(0.95f)
                                      : Color::rgb(0x132242).with_alpha(0.8f));
        paint.stroke(web_chip, 22.0f, web_focused ? 2.5f : 1.5f,
                     web_focused ? theme.primary
                                 : (server_running ? Color::rgb(0x38ef7d).with_alpha(0.5f)
                                                   : Color::rgb(0x76d6ff).with_alpha(0.3f)));
        if (web_focused)
            paint.focus_ring(web_chip, 22.0f, focus_amount_[kBtnWebServerHeader].value);

        list.circle(web_chip.x + 22, web_chip.cy(), 5.0f,
                    server_running ? Color::rgb(0x38ef7d) : Color::rgb(0x76d6ff));
        char web_pill_text[64];
        if (server_running)
        {
            std::snprintf(web_pill_text, sizeof(web_pill_text), "%s (:%u)",
                          machete::i18n::tr(machete::i18n::StringId::WebServerActive),
                          static_cast<unsigned>(machete::server::WebServer::instance().port()));
        }
        else
        {
            std::snprintf(web_pill_text, sizeof(web_pill_text), "%s",
                          machete::i18n::tr(machete::i18n::StringId::WebServerPrompt));
        }
        paint.body(web_pill_text,
                   web_chip.x + 36, web_chip.cy() + 7, 18,
                   web_focused ? theme.primary : Color::rgb(0xffffff));

        // ==========================================
        // 2. PANEL IZQUIERDO: LISTA DINAMICA DE JUEGOS
        // ==========================================
        const Rect left_panel{96, 210, 680, 750};
        paint.panel(left_panel);

        const bool is_es = (machete::i18n::current_language() == machete::i18n::Language::Spanish);
        paint.label(is_es ? "PARTIDAS Y JUEGOS DETECTADOS" : "DETECTED GAMES & SAVE DATA",
                    126, 246, 17, theme.text_muted);
        paint.label("[L1 / R1]", 736, 246, 15, theme.primary.with_alpha(0.85f), gfx::Align::right);

        int ps5_count = 0, ps4_count = 0, all_count = 0;
        get_tab_counts(ps5_count, ps4_count, all_count);

        // Barra / Dock de pestañas con fondo translúcido Liquid Glass
        const Rect tabs_dock{112, 262, 648, 50};
        list.rounded_rect(tabs_dock, 12.0f, Color::rgb(0x0c162c).with_alpha(0.7f));
        paint.stroke(tabs_dock, 12.0f, 1.0f, Color::rgb(0xffffff).with_alpha(0.08f));

        // Indicador deslizante animado (Spring physics) de la pestaña activa
        const float ind_x = 116.0f + tab_indicator_.value * 216.0f;
        const Rect slider_rect{ind_x, 266.0f, 206.0f, 42.0f};
        list.shadow(slider_rect, 10.0f, 8.0f, theme.primary.with_alpha(0.25f));
        list.rounded_rect(slider_rect, 10.0f, theme.primary.with_alpha(0.22f));
        paint.stroke(slider_rect, 10.0f, 1.5f, theme.primary.with_alpha(0.75f));

        // Pestaña PS5
        {
            const Rect r_ps5 = kRects[kTabPS5];
            const bool f_ps5 = (focus_ == kTabPS5);
            const bool act_ps5 = (current_tab_ == GameTab::PS5);
            if (f_ps5)
                paint.focus_ring(r_ps5, 10.0f, focus_amount_[kTabPS5].value);
            paint.body("PS5", r_ps5.x + 24.0f, r_ps5.cy() + 7.0f, 18,
                       act_ps5 ? theme.text : theme.text_muted);
            const Rect badge{r_ps5.x + r_ps5.w - 52.0f, r_ps5.cy() - 11.0f, 40.0f, 22.0f};
            list.rounded_rect(badge, 11.0f, act_ps5 ? theme.primary.with_alpha(0.35f) : Color::rgb(0xffffff).with_alpha(0.08f));
            char b_str[16];
            std::snprintf(b_str, sizeof(b_str), "%d", ps5_count);
            paint.label(b_str, badge.cx(), badge.cy() + 5.0f, 14,
                        act_ps5 ? theme.primary : theme.text_muted, gfx::Align::center);
        }

        // Pestaña PS4
        {
            const Rect r_ps4 = kRects[kTabPS4];
            const bool f_ps4 = (focus_ == kTabPS4);
            const bool act_ps4 = (current_tab_ == GameTab::PS4);
            if (f_ps4)
                paint.focus_ring(r_ps4, 10.0f, focus_amount_[kTabPS4].value);
            paint.body("PS4", r_ps4.x + 24.0f, r_ps4.cy() + 7.0f, 18,
                       act_ps4 ? theme.text : theme.text_muted);
            const Rect badge{r_ps4.x + r_ps4.w - 52.0f, r_ps4.cy() - 11.0f, 40.0f, 22.0f};
            list.rounded_rect(badge, 11.0f, act_ps4 ? theme.primary.with_alpha(0.35f) : Color::rgb(0xffffff).with_alpha(0.08f));
            char b_str[16];
            std::snprintf(b_str, sizeof(b_str), "%d", ps4_count);
            paint.label(b_str, badge.cx(), badge.cy() + 5.0f, 14,
                        act_ps4 ? theme.primary : theme.text_muted, gfx::Align::center);
        }

        // Pestaña TODOS
        {
            const Rect r_all = kRects[kTabAll];
            const bool f_all = (focus_ == kTabAll);
            const bool act_all = (current_tab_ == GameTab::All);
            if (f_all)
                paint.focus_ring(r_all, 10.0f, focus_amount_[kTabAll].value);
            paint.body(is_es ? "TODOS" : "ALL", r_all.x + 20.0f, r_all.cy() + 7.0f, 18,
                       act_all ? theme.text : theme.text_muted);
            const Rect badge{r_all.x + r_all.w - 54.0f, r_all.cy() - 11.0f, 42.0f, 22.0f};
            list.rounded_rect(badge, 11.0f, act_all ? theme.primary.with_alpha(0.35f) : Color::rgb(0xffffff).with_alpha(0.08f));
            char b_str[16];
            std::snprintf(b_str, sizeof(b_str), "%d", all_count);
            paint.label(b_str, badge.cx(), badge.cy() + 5.0f, 14,
                        act_all ? theme.primary : theme.text_muted, gfx::Align::center);
        }

        // Lista de juegos filtrados por pestaña activa
        const auto filtered = get_filtered_game_indices();
        const int total_games = static_cast<int>(filtered.size());
        if (total_games == 0)
        {
            const Rect empty_box{126, 340, 620, 160};
            list.rounded_rect(empty_box, theme.radius, Color::rgb(0xffffff).with_alpha(0.04f));
            paint.stroke(empty_box, theme.radius, 1.0f, theme.outline);
            const char *empty_title = (current_tab_ == GameTab::PS5)
                ? (is_es ? "No se detectaron juegos de PS5" : "No PS5 games detected")
                : ((current_tab_ == GameTab::PS4)
                       ? (is_es ? "No se detectaron juegos de PS4" : "No PS4 games detected")
                       : (is_es ? "No se detectaron juegos instalados" : "No installed games detected"));
            paint.heading(empty_title, empty_box.cx(), empty_box.cy() - 14, 24, theme.text, gfx::Align::center);
            paint.body(is_es ? "Usa L1/R1 para cambiar o presiona Cuadrado para reescanear"
                             : "Use L1/R1 to change tabs or press Square to re-scan",
                       empty_box.cx(), empty_box.cy() + 18, 18, theme.text_muted, gfx::Align::center);
        }

        const int visible_count = std::min(5, total_games);

        for (int i = 0; i < visible_count; ++i)
        {
            const int filtered_idx = scroll_offset_ + i;
            if (filtered_idx >= total_games)
                break;

            const int real_game_idx = filtered[static_cast<std::size_t>(filtered_idx)];
            const auto &game = games_[static_cast<std::size_t>(real_game_idx)];
            const Rect r = kRects[kGame0 + i];
            const bool is_focused = (focus_ == kGame0 + i);
            const bool is_selected = (selected_game_ == filtered_idx);

            // Fondo de la fila
            if (is_focused)
            {
                paint.surface(r, theme.radius, theme.surface_high, theme.outline, 1.0f);
                paint.focus_ring(r, theme.radius, focus_amount_[kGame0 + i].value);
            }
            else if (is_selected)
            {
                list.rounded_rect(r, theme.radius, theme.primary.with_alpha(0.18f));
                list.line(r.x + 2, r.y + 12, r.x + 2, r.y + r.h - 12, 4.0f, theme.primary);
            }
            else
            {
                list.rounded_rect(r, theme.radius, Color::rgb(0xffffff).with_alpha(0.04f));
            }

            // Icono Real del juego (icon0.png) con fallback suave
            const Rect icon_rect{r.x + 14, r.y + 14, 64, 64};
            if (game.icon_texture != 0)
            {
                list.image(game.icon_texture, icon_rect, gfx::kFullUv, Color::rgb(0xffffff), 8.0f);
                paint.stroke(icon_rect, 8.0f, 1.0f, Color::rgb(0xffffff).with_alpha(0.2f));
            }
            else
            {
                // Tarjeta monocromática estilizada
                list.rounded_rect(icon_rect, 8.0f, Color::rgb(0x142b4d).with_alpha(0.85f));
                paint.stroke(icon_rect, 8.0f, 1.0f, Color::rgb(0x76d6ff).with_alpha(0.3f));
                char init[4] = "PS";
                if (!game.title.empty() && static_cast<unsigned char>(game.title[0]) >= 32 &&
                    static_cast<unsigned char>(game.title[0]) <= 126)
                {
                    init[0] = game.title[0];
                    init[1] = '\0';
                }
                paint.label(init, icon_rect.cx(), icon_rect.cy() + 7, 24, Color::rgb(0x76d6ff),
                            gfx::Align::center);
            }

            // Título oficial y metadatos con salvaguarda
            const std::string title_disp = !game.title.empty() ? game.title : game.title_id;
            paint.heading(title_disp.c_str(), r.x + 92, r.y + 44, 24,
                          is_focused ? theme.text : theme.text.with_alpha(0.9f));
            char sub[128];
            std::snprintf(sub, sizeof(sub), "%s  -  %s  -  %s",
                          game.platform.empty() ? "PS5" : game.platform.c_str(),
                          game.title_id.c_str(),
                          game.size_str.empty() ? "Instalado" : game.size_str.c_str());
            paint.body(sub, r.x + 92, r.y + 70, 18, theme.text_muted);
        }

        // ==========================================
        // 3. PANEL DERECHO: DETALLES Y ACCIONES
        // ==========================================
        const Rect right_panel{806, 210, 1018, 750};
        paint.panel(right_panel);

        const auto *current_game_ptr = current_selected_game();
        if (current_game_ptr != nullptr)
        {
            const auto &current_game = *current_game_ptr;

            // Carátula / Icono grande (196x196)
            const Rect big_cover{836, 256, 196, 196};
            if (current_game.icon_texture != 0)
            {
                list.image(current_game.icon_texture, big_cover, gfx::kFullUv,
                           Color::rgb(0xffffff), 14.0f);
                paint.stroke(big_cover, 14.0f, 1.5f, Color::rgb(0xffffff).with_alpha(0.25f));
            }
            else
            {
                list.rounded_rect(big_cover, 14.0f, Color::rgb(0x142b4d).with_alpha(0.85f));
                paint.stroke(big_cover, 14.0f, 2.0f, Color::rgb(0x76d6ff).with_alpha(0.4f));
                char init[4] = "PS";
                if (!current_game.title.empty() && static_cast<unsigned char>(current_game.title[0]) >= 32 &&
                    static_cast<unsigned char>(current_game.title[0]) <= 126)
                {
                    init[0] = current_game.title[0];
                    init[1] = '\0';
                }
                paint.heading(init, big_cover.cx(), big_cover.cy() + 20, 68,
                              Color::rgb(0x76d6ff), gfx::Align::center);
            }

            // Metadatos principales
            const Rect tag_rect{1058, 256, 110, 30};
            list.rounded_rect(tag_rect, 6.0f, Color::rgb(0x194582).with_alpha(0.7f));
            paint.label(current_game.platform.empty() ? "PS5" : current_game.platform.c_str(),
                        tag_rect.cx(), tag_rect.cy() + 6, 16,
                        Color::rgb(0x76d6ff), gfx::Align::center);

            const bool is_ps4_title = (current_game.platform == "PS4");
            if (is_ps4_title)
            {
                const Rect apollo_tag{1178, 256, 210, 30};
                list.rounded_rect(apollo_tag, 6.0f, Color::rgb(0x143825).with_alpha(0.9f));
                paint.stroke(apollo_tag, 6.0f, 1.0f, Color::rgb(0x38ef7d).with_alpha(0.6f));
                paint.label("APOLLO RESIGN ACTIVO", apollo_tag.cx(), apollo_tag.cy() + 6, 14,
                            Color::rgb(0x38ef7d), gfx::Align::center);
                paint.label(current_game.title_id.c_str(), 1400, 276, 18, theme.text_muted);
            }
            else
            {
                paint.label(current_game.title_id.c_str(), 1184, 276, 18, theme.text_muted);
            }
            const std::string current_title_disp = !current_game.title.empty() ? current_game.title : current_game.title_id;
            paint.heading(current_title_disp.c_str(), 1058, 334, 38, theme.text);

            char slot_info[128];
            std::snprintf(slot_info, sizeof(slot_info),
                          machete::i18n::tr(machete::i18n::StringId::StatusPrefix),
                          current_game.size_str.empty() ? "Instalado" : current_game.size_str.c_str());
            paint.body(slot_info, 1058, 372, 22, theme.text_muted);

            char save_time[128];
            std::snprintf(save_time, sizeof(save_time),
                          machete::i18n::tr(machete::i18n::StringId::LastActivityPrefix),
                          current_game.timestamp_str.empty() ? "Listo para respaldar" : current_game.timestamp_str.c_str());
            paint.body(save_time, 1058, 404, 20, theme.primary);

            // Separador
            list.line(836, 480, 1784, 480, 1.0f, theme.text_muted.with_alpha(0.2f));

            // Estado del Keystone e Inyeccion Cross-Console / Apollo
            paint.label(machete::i18n::tr(machete::i18n::StringId::KeystoneStatusHeader),
                        836, 512, 17, theme.text_muted);
            list.rounded_rect({836, 526, 948, 78}, 8.0f, Color::rgb(0x142b1f).with_alpha(0.6f));
            list.circle(860, 548, 6.0f,
                        current_game.has_keystone ? Color::rgb(0x38ef7d) : Color::rgb(0xffaa00));
            paint.body(current_game.has_keystone
                           ? machete::i18n::tr(machete::i18n::StringId::KeystoneStatusOk)
                           : machete::i18n::tr(machete::i18n::StringId::KeystoneStatusPending),
                       880, 555, 19, Color::rgb(0xa3ffcc));

            const bool is_es = (machete::i18n::current_language() == machete::i18n::Language::Spanish);
            const bool dummy_ready = is_ps4_title || current_game.has_save_dummy || !current_game.save_path.empty();
            list.circle(860, 580, 6.0f,
                        dummy_ready ? Color::rgb(0x38ef7d) : Color::rgb(0xffaa00));
            const char *cross_msg = is_ps4_title
                ? (is_es ? "Modo Apollo PS4: Resignado Automatico Activo (Account ID + Keystone + PFS)"
                         : "Apollo PS4 Mode: Auto-Resign Active (Account ID + Keystone + PFS)")
                : (dummy_ready
                       ? (is_es ? "Save Dummy PS5: LISTO (Inyeccion Cross-Console Activa sin corrupcion)"
                                : "PS5 Save Dummy: READY (Zero-Corruption Cross-Console Injection Active)")
                       : (is_es ? "Save Dummy PS5: Abre el juego 1 vez antes de restaurar de otra consola"
                                : "PS5 Save Dummy: Launch game once before restoring from another console"));
            paint.body(cross_msg, 880, 587, 18, dummy_ready ? Color::rgb(0x76d6ff) : Color::rgb(0xffd276));

            // Destinos de almacenamiento disponibles
            paint.label(machete::i18n::tr(machete::i18n::StringId::StorageLocationsHeader),
                        836, 630, 17, theme.text_muted);
            std::string web_loc_str;
            auto &web_inst = machete::server::WebServer::instance();
            if (web_inst.is_running())
            {
                web_loc_str = "Web: " + web_inst.url();
            }
            else
            {
                web_loc_str = machete::i18n::tr(machete::i18n::StringId::LocWebServer);
            }
            const char *locs[] = {
                machete::i18n::tr(machete::i18n::StringId::LocInternal),
                machete::i18n::tr(machete::i18n::StringId::LocM2),
                machete::i18n::tr(machete::i18n::StringId::LocUsb),
                web_loc_str.c_str(),
            };
            for (int l = 0; l < 4; ++l)
            {
                const Rect lr{836.0f + static_cast<float>(l % 2) * 480.0f,
                              646.0f + static_cast<float>(l / 2) * 44.0f, 460.0f, 36.0f};
                list.rounded_rect(lr, 6.0f, Color::rgb(0xffffff).with_alpha(0.05f));
                paint.body(locs[l], lr.x + 14, lr.cy() + 7, 18, theme.text_muted);
            }

            // Botones de acción
            paint.label(machete::i18n::tr(machete::i18n::StringId::SaveActionsHeader),
                        836, 756, 17, theme.text_muted);
            paint.button(kRects[kBtnBackup], machete::i18n::tr(machete::i18n::StringId::BtnBackup),
                         ui::ButtonKind::primary, look(kBtnBackup));
            const char *restore_btn_label = is_ps4_title
                ? (is_es ? "Restaurar (Modo Apollo)" : "Restore (Apollo Mode)")
                : machete::i18n::tr(machete::i18n::StringId::BtnRestore);
            paint.button(kRects[kBtnRestore], restore_btn_label,
                         ui::ButtonKind::secondary, look(kBtnRestore));
            paint.button(kRects[kBtnWebServer], machete::i18n::tr(machete::i18n::StringId::BtnWebServer),
                         ui::ButtonKind::secondary, look(kBtnWebServer));
        }
        else
        {
            const bool is_es = (machete::i18n::current_language() == machete::i18n::Language::Spanish);
            paint.heading(is_es ? "Estado de MacheteSVG y Almacenamiento" : "MacheteSVG System & Storage",
                          856, 280, 32, theme.text);
            paint.body(is_es ? "El gestor de partidas nativo para PS5 y PS4 esta listo para operar."
                             : "Native save data manager for PS5 & PS4 is ready to operate.",
                       856, 324, 20, theme.text_muted);

            const char *guide_steps[] = {
                is_es ? "1. Privilegios: Observa el chip superior (Root / Sandbox)."
                      : "1. Privileges: Check upper badge (Root / Sandbox).",
                is_es ? "2. Deteccion: Con permisos root, lee automaticamente /user/home y /user/app."
                      : "2. Detection: With root privileges, scans /user/home and /user/app automatically.",
                is_es ? "3. Pendrive USB: Conecta un dispositivo exFAT para respaldos portables."
                      : "3. USB Drive: Connect an exFAT device for portable offline backups.",
                is_es ? "4. Servidor Web: Presiona Triangulo para descargar partidas por Wi-Fi / LAN."
                      : "4. Web Server: Press Triangle to download savegames over Wi-Fi / LAN.",
                is_es ? "5. Reescanear: Presiona Cuadrado para refrescar los juegos y partidas."
                      : "5. Refresh: Press Square to re-scan games and save data.",
            };
            for (int s = 0; s < 5; ++s)
            {
                const Rect sr{856, 370.0f + static_cast<float>(s) * 56.0f, 918, 46};
                list.rounded_rect(sr, 8.0f, Color::rgb(0xffffff).with_alpha(0.04f));
                list.circle(sr.x + 20, sr.cy(), 5.0f, theme.primary);
                paint.body(guide_steps[s], sr.x + 36, sr.cy() + 6, 18, theme.text);
            }
        }

        // Toast de notificación (solo si no hay un diálogo de alerta activo)
        if (notification_timer_ > 0.0f && alert_dialog_show_.value <= 0.01f)
        {
            const Rect toast{560, 40, 800, 54};
            list.rounded_rect(toast, 27.0f, Color::rgb(0x0e271a).with_alpha(0.95f));
            paint.stroke(toast, 27.0f, 1.5f, Color::rgb(0x38ef7d).with_alpha(0.6f));
            paint.body(notification_text_, toast.cx(), toast.cy() + 7, 20, Color::rgb(0xffffff),
                       gfx::Align::center);
        }

        // ==========================================
        // 4. DIALOGO MODAL: SERVIDOR WEB LOCAL
        // ==========================================
        const float ws_show = web_server_dialog_show_.value;
        if (ws_show > 0.01f)
        {
            list.rounded_rect({0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0,
                              Color::rgb(0x000000).with_alpha(0.7f * ws_show));
            list.push_opacity(tween::clamp01(ws_show));
            list.push_transform(0.94f + 0.06f * tween::clamp01(ws_show), 960, 540, 0, 0);

            const Rect dlg{380, 170, 1160, 740};
            paint.panel(dlg);

            auto &server = machete::server::WebServer::instance();
            const bool is_running = server.is_running();
            const bool is_es = (machete::i18n::current_language() == machete::i18n::Language::Spanish);

            // Columna Izquierda: Titulos y controles de servidor
            paint.heading(machete::i18n::tr(machete::i18n::StringId::WebServerDialogTitle),
                          425, 235, 32, theme.text);
            paint.body(machete::i18n::tr(machete::i18n::StringId::WebServerDialogSubtitle),
                       425, 270, 18, theme.text_muted);

            // Estado del servidor (Pildora / Badge de estado)
            const Rect status_badge{425, 305, 650, 46};
            list.rounded_rect(status_badge, 10.0f,
                              is_running ? Color::rgb(0x0a2d1f).with_alpha(0.9f)
                                         : Color::rgb(0x2a2412).with_alpha(0.9f));
            paint.stroke(status_badge, 10.0f, 1.5f,
                         is_running ? Color::rgb(0x38ef7d).with_alpha(0.7f)
                                    : Color::rgb(0xffaa00).with_alpha(0.7f));
            list.circle(status_badge.x + 24, status_badge.cy(), 6.0f,
                        is_running ? Color::rgb(0x38ef7d) : Color::rgb(0xffaa00));
            paint.body(is_running ? machete::i18n::tr(machete::i18n::StringId::WebServerStatusRunning)
                                  : machete::i18n::tr(machete::i18n::StringId::WebServerStatusStopped),
                       status_badge.x + 44, status_badge.cy() + 7, 20,
                       is_running ? Color::rgb(0x38ef7d) : Color::rgb(0xffaa00));

            // Caja con la Direccion URL para el navegador
            paint.label(machete::i18n::tr(machete::i18n::StringId::WebServerUrlLabel),
                        425, 375, 16, theme.text_muted);
            const Rect url_box{425, 392, 650, 64};
            list.rounded_rect(url_box, 10.0f,
                              is_running ? Color::rgb(0x0e2744).with_alpha(0.95f)
                                         : Color::rgb(0x1a2130).with_alpha(0.85f));
            paint.stroke(url_box, 10.0f, 2.0f,
                         is_running ? Color::rgb(0x76d6ff).with_alpha(0.8f)
                                    : Color::rgb(0x556677).with_alpha(0.4f));

            const std::string s_url = is_running
                ? server.url()
                : ("http://<IP-PS5>:" + std::to_string(machete::server::kDefaultPort));
            paint.heading(s_url.c_str(), url_box.cx(), url_box.cy() + 11, 28,
                          is_running ? Color::rgb(0x76d6ff) : Color::rgb(0x8899aa),
                          gfx::Align::center);

            // Instrucciones descriptivas
            const Rect info_box{425, 475, 650, 160};
            list.rounded_rect(info_box, 10.0f, Color::rgb(0xffffff).with_alpha(0.04f));
            paint.stroke(info_box, 10.0f, 1.0f, Color::rgb(0xffffff).with_alpha(0.08f));

            const char *line1 = is_es
                ? "1. Abre el navegador o escanea el codigo QR con tu telefono o tablet."
                : "1. Open your browser or scan the QR code with your phone or tablet.";
            const char *line2 = is_es
                ? "2. Gestiona y descarga tus partidas en ZIP o restaura copias con un clic."
                : "2. Manage and download your saves in ZIP or restore backups in one click.";
            const char *line3 = is_es
                ? "3. Transferencias directas en red local a maxima velocidad sin servicios cloud."
                : "3. Direct LAN transfers at maximum speed with zero cloud service dependencies.";

            paint.body(line1, info_box.x + 24, info_box.y + 36, 17, theme.text);
            paint.body(line2, info_box.x + 24, info_box.y + 74, 17, theme.text);
            paint.body(line3, info_box.x + 24, info_box.y + 112, 17, theme.text_muted);

            // Columna Derecha: Tarjeta con Codigo QR (ISO/IEC 18004 con Quiet Zone)
            const Rect qr_card{1115, 235, 385, 400};
            list.rounded_rect(qr_card, 16.0f, Color::rgb(0xffffff));
            paint.stroke(qr_card, 16.0f, 1.5f,
                         is_running ? Color::rgb(0x76d6ff).with_alpha(0.6f)
                                    : Color::rgb(0xcccccc).with_alpha(0.4f));

            if (cached_qr_url_ != s_url)
            {
                cached_qr_url_ = s_url;
                cached_qr_matrix_ = {};
                hui::ui::qr::encode_text(s_url, cached_qr_matrix_);
            }

            if (cached_qr_matrix_.size > 0)
            {
                const int qsize = cached_qr_matrix_.size;
                const float qr_box_size = 280.0f;
                const float mod_size = std::floor(qr_box_size / static_cast<float>(qsize));
                const float qr_actual_size = mod_size * static_cast<float>(qsize);
                const float qr_offset_x = qr_card.x + (qr_card.w - qr_actual_size) * 0.5f;
                const float qr_offset_y = qr_card.y + 24.0f;

                for (int qy = 0; qy < qsize; ++qy)
                {
                    for (int qx = 0; qx < qsize; ++qx)
                    {
                        if (cached_qr_matrix_.get(qx, qy))
                        {
                            list.rounded_rect({qr_offset_x + static_cast<float>(qx) * mod_size,
                                               qr_offset_y + static_cast<float>(qy) * mod_size,
                                               mod_size, mod_size}, 0.0f, Color::rgb(0x0a0f1d));
                        }
                    }
                }

                paint.heading(is_es ? "Escanea con tu camara" : "Scan with your camera",
                              qr_card.cx(), qr_offset_y + qr_actual_size + 36.0f, 18,
                              Color::rgb(0x0a0f1d), gfx::Align::center);
                paint.body(is_es ? "Acceso instantaneo a la Estacion Web" : "Instant access to Web Save Station",
                           qr_card.cx(), qr_offset_y + qr_actual_size + 60.0f, 14,
                           Color::rgb(0x4a5568), gfx::Align::center);
            }

            // Botones interactivos (Iniciar/Detener y Cerrar)
            const Rect btn_action{425, 655, 520, 54};
            ui::Look action_look;
            action_look.focus = (web_server_dialog_focus_ == 0) ? 1.0f : 0.0f;
            const char *action_text = is_running
                ? machete::i18n::tr(machete::i18n::StringId::WebServerBtnStop)
                : machete::i18n::tr(machete::i18n::StringId::WebServerBtnStart);
            paint.button(btn_action, action_text,
                         is_running ? ui::ButtonKind::secondary : ui::ButtonKind::primary,
                         action_look);

            const Rect btn_close{980, 655, 520, 54};
            ui::Look close_look;
            close_look.focus = (web_server_dialog_focus_ == 1) ? 1.0f : 0.0f;
            paint.button(btn_close, machete::i18n::tr(machete::i18n::StringId::WebServerClose),
                         ui::ButtonKind::secondary, close_look);

            list.pop_transform();
            list.pop_opacity();
        }

        // ==========================================
        // 5. DIALOGO MODAL: SELECTOR DE DESTINO / RESTAURACION
        // ==========================================
        const float show = dialog_show_.value;
        if (show > 0.01f)
        {
            list.rounded_rect({0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0,
                              Color::rgb(0x000000).with_alpha(0.68f * show));
            list.push_opacity(tween::clamp01(show));
            list.push_transform(0.94f + 0.06f * tween::clamp01(show), 960, 540, 0, 0);

            const bool is_es = (machete::i18n::current_language() == machete::i18n::Language::Spanish);
            const Rect dlg{460, 190, 1000, 700};
            paint.panel(dlg);

            const auto *curr_game_ptr = current_selected_game();
            const std::string curr_title = (curr_game_ptr != nullptr)
                ? (!curr_game_ptr->title.empty()
                       ? curr_game_ptr->title
                       : curr_game_ptr->title_id)
                : "";

            if (dialog_is_backup_)
            {
                // CABECERA MODAL RESPALDO
                paint.heading(is_es ? "DESTINO DE COPIA DE SEGURIDAD" : "BACKUP DESTINATION",
                              500, 252, 34, theme.text);
                char sub[160];
                std::snprintf(sub, sizeof(sub),
                              is_es ? "Guardar partida de %s en:" : "Save backup for %s to:",
                              curr_title.c_str());
                paint.body(sub, 500, 288, 20, theme.text_muted);

                // LISTA DE DISPOSITIVOS DE ALMACENAMIENTO
                const int opt_count = static_cast<int>(active_devices_.size());
                const float item_h = 74.0f;
                for (int s = 0; s < opt_count; ++s)
                {
                    const auto &dev = active_devices_[static_cast<std::size_t>(s)];
                    const Rect opt_rect{500, 320.0f + static_cast<float>(s) * item_h, 920, 64};
                    const bool is_focused = (dialog_focus_ == s);

                    if (is_focused)
                    {
                        paint.surface(opt_rect, theme.radius, theme.surface_high, theme.primary, 1.0f);
                        paint.focus_ring(opt_rect, theme.radius, 1.0f);
                    }
                    else
                    {
                        list.rounded_rect(opt_rect, theme.radius, Color::rgb(0xffffff).with_alpha(0.06f));
                    }

                    list.circle(opt_rect.x + 24, opt_rect.cy(), 6.0f,
                                dev.connected ? Color::rgb(0x38ef7d) : Color::rgb(0x76d6ff));

                    paint.body(dev.label.c_str(), opt_rect.x + 44, opt_rect.cy() - 6, 21,
                               is_focused ? theme.text : theme.text.with_alpha(0.9f));

                    const std::string path_desc = dev.root_dir;
                    paint.label(path_desc.c_str(), opt_rect.x + 44, opt_rect.cy() + 18, 16,
                                theme.text_muted);

                    paint.label(dev.free_space_formatted().c_str(), opt_rect.x + opt_rect.w - 24,
                                opt_rect.cy() + 6, 17,
                                is_focused ? theme.primary : Color::rgb(0x76d6ff),
                                gfx::Align::right);
                }

                // Boton Cancelar
                const Rect cancel_btn{500, 320.0f + static_cast<float>(opt_count) * item_h + 16.0f, 920, 54};
                ui::Look cancel_look;
                cancel_look.focus = (dialog_focus_ == opt_count) ? 1.0f : 0.0f;
                paint.button(cancel_btn, is_es ? "Cancelar" : "Cancel", ui::ButtonKind::secondary, cancel_look);
            }
            else
            {
                // CABECERA MODAL RESTAURACION
                const bool is_ps4_curr = (curr_game_ptr != nullptr) && (curr_game_ptr->platform == "PS4");
                paint.heading(is_ps4_curr
                                  ? (is_es ? "RESTAURAR Y RESIGNAR (MODO APOLLO PS4)" : "RESTORE & RESIGN (APOLLO PS4 MODE)")
                                  : (is_es ? "RESTAURAR COPIA DE SEGURIDAD" : "RESTORE SAVE BACKUP"),
                              500, 252, 34, theme.text);

                if (available_backups_.empty())
                {
                    // ESTADO SIN COPIAS DISPONIBLES
                    char sub[160];
                    std::snprintf(sub, sizeof(sub),
                                  is_es ? "No se encontraron copias para %s" : "No backups found for %s",
                                  curr_title.c_str());
                    paint.body(sub, 500, 288, 20, theme.text_muted);

                    const Rect empty_box{500, 340, 920, 220};
                    list.rounded_rect(empty_box, theme.radius, Color::rgb(0xffffff).with_alpha(0.04f));
                    paint.stroke(empty_box, theme.radius, 1.0f, theme.outline);

                    list.circle(empty_box.cx(), empty_box.y + 45, 18.0f, Color::rgb(0xffaa00).with_alpha(0.2f));
                    paint.heading("!", empty_box.cx(), empty_box.y + 53, 26, Color::rgb(0xffaa00), gfx::Align::center);

                    paint.heading(is_es ? "No hay respaldos disponibles" : "No backups available",
                                  empty_box.cx(), empty_box.y + 100, 24, theme.text, gfx::Align::center);
                    paint.body(is_es ? "Para restaurar una partida, primero realiza un respaldo o coloca la carpeta en:"
                                     : "To restore a save, first create a backup or place folder in:",
                               empty_box.cx(), empty_box.y + 135, 18, theme.text_muted, gfx::Align::center);
                    paint.label("/mnt/usb0/MacheteSVG/ o /data/MacheteSVG/",
                                empty_box.cx(), empty_box.y + 168, 18, Color::rgb(0x76d6ff), gfx::Align::center);

                    const Rect close_btn{500, 600, 920, 54};
                    ui::Look close_look;
                    close_look.focus = 1.0f;
                    paint.button(close_btn, is_es ? "Cerrar / Volver" : "Close / Back",
                                 ui::ButtonKind::primary, close_look);
                }
                else
                {
                    // LISTA DE COPIAS DISPONIBLES
                    const bool has_curr = (curr_game_ptr != nullptr);
                    const bool is_ps4_restore = has_curr && (curr_game_ptr->platform == "PS4");
                    const bool has_dummy_restore = is_ps4_restore ||
                        (has_curr && (curr_game_ptr->has_save_dummy ||
                                      !curr_game_ptr->save_path.empty()));
                    const char *guide_line = is_ps4_restore
                        ? (is_es ? "Modo Apollo PS4: Resigna Account ID, PSID y Keystone automaticamente al restaurar."
                                 : "Apollo PS4 Mode: Automatically resigns Account ID, PSID and Keystone on restore.")
                        : (has_dummy_restore
                               ? (is_es ? "Save Dummy Detectado: Inyeccion PFS automatica lista para esta u otra PS5."
                                        : "Save Dummy Detected: Automatic PFS injection ready for this or another PS5.")
                               : (is_es ? "ATENCION: Si restauras de otra PS5, abre el juego 1 vez primero para crear Save Dummy."
                                        : "NOTICE: If restoring from another PS5, launch game once first to create Save Dummy."));
                    paint.body(guide_line, 500, 288, 18,
                               has_dummy_restore ? Color::rgb(0x76d6ff) : Color::rgb(0xffaa00));

                    const int opt_count = static_cast<int>(available_backups_.size());
                    const float item_h = 74.0f;
                    for (int s = 0; s < opt_count; ++s)
                    {
                        const auto &item = available_backups_[static_cast<std::size_t>(s)];
                        const Rect opt_rect{500, 320.0f + static_cast<float>(s) * item_h, 920, 64};
                        const bool is_focused = (dialog_focus_ == s);

                        if (is_focused)
                        {
                            paint.surface(opt_rect, theme.radius, theme.surface_high, theme.primary, 1.0f);
                            paint.focus_ring(opt_rect, theme.radius, 1.0f);
                        }
                        else
                        {
                            list.rounded_rect(opt_rect, theme.radius, Color::rgb(0xffffff).with_alpha(0.06f));
                        }

                        // Badge de origen
                        const Rect dev_badge{opt_rect.x + 14, opt_rect.y + 14, 110, 36};
                        const Color badge_col = (item.storage_type == machete::storage::StorageType::USB)
                            ? Color::rgb(0x195282)
                            : ((item.storage_type == machete::storage::StorageType::SSD)
                                ? Color::rgb(0x502878)
                                : Color::rgb(0x1a4533));
                        list.rounded_rect(dev_badge, 6.0f, badge_col.with_alpha(0.85f));
                        paint.label(item.device_label.c_str(), dev_badge.cx(), dev_badge.cy() + 6, 15,
                                    Color::rgb(0xffffff), gfx::Align::center);

                        // Nombre de carpeta
                        paint.body(item.folder_name.c_str(), opt_rect.x + 140, opt_rect.cy() - 6, 21,
                                   is_focused ? theme.text : theme.text.with_alpha(0.9f));

                        // Metadata
                        char meta_str[128];
                        std::snprintf(meta_str, sizeof(meta_str), "%s  |  %d archivos  |  %s",
                                      item.timestamp_str.c_str(), item.file_count, item.size_str.c_str());
                        paint.label(meta_str, opt_rect.x + 140, opt_rect.cy() + 18, 16, theme.text_muted);

                        // Indicador Cross-Console / Keystone / Apollo
                        if (item.has_decrypted || item.has_crypto_vault || item.has_keystone || is_ps4_restore)
                        {
                            const Rect k_badge{opt_rect.x + opt_rect.w - 185, opt_rect.cy() - 14, 170, 28};
                            list.rounded_rect(k_badge, 6.0f, Color::rgb(0x123620).with_alpha(0.85f));
                            const char *b_lbl = is_ps4_restore
                                ? "MODO APOLLO OK"
                                : ((item.has_decrypted || item.has_crypto_vault) ? "CROSS-CONSOLE OK" : "KEYSTONE OK");
                            paint.label(b_lbl, k_badge.cx(), k_badge.cy() + 6, 13,
                                        Color::rgb(0x38ef7d), gfx::Align::center);
                        }
                    }

                    // Boton Cancelar
                    const Rect cancel_btn{500, 320.0f + static_cast<float>(opt_count) * item_h + 16.0f, 920, 54};
                    ui::Look cancel_look;
                    cancel_look.focus = (dialog_focus_ == opt_count) ? 1.0f : 0.0f;
                    paint.button(cancel_btn, is_es ? "Cancelar" : "Cancel", ui::ButtonKind::secondary, cancel_look);
                }
            }

            list.pop_transform();
            list.pop_opacity();
        }

        // ==========================================
        // 5.5. MODAL DE PROGRESO DE OPERACIÓN ASÍNCRONA (Apple Liquid Glass)
        // ==========================================
        const float op_show = op_overlay_show_.value;
        if (op_show > 0.005f)
        {
            list.rounded_rect({0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0,
                              Color::rgb(0x000000).with_alpha(0.72f * op_show));
            list.push_opacity(tween::clamp01(op_show));
            list.push_transform(0.92f + 0.08f * tween::clamp01(op_show), 960, 540, 0, 0);

            const Rect card{520, 310, 880, 460};
            paint.panel(card);

            std::string title_str;
            std::string detail_str;
            {
                std::lock_guard<std::mutex> lock(op_mutex_);
                title_str = operation_title_;
                detail_str = operation_detail_;
            }

            const bool is_es = (machete::i18n::current_language() == machete::i18n::Language::Spanish);
            const float progress = tween::clamp01(op_progress_bar_.value);
            const int pct = static_cast<int>(std::round(progress * 100.0f));

            // Spinner animado (anillo con satélite orbital)
            const float spin_cx = 960.0f;
            const float spin_cy = 390.0f;
            const float spin_r = 30.0f;
            list.circle(spin_cx, spin_cy, spin_r, theme.primary.with_alpha(0.18f));
            const float rad = op_spinner_angle_ * (3.14159265f / 180.0f);
            const float sat_x = spin_cx + std::cos(rad) * spin_r;
            const float sat_y = spin_cy + std::sin(rad) * spin_r;
            list.circle(sat_x, sat_y, 6.0f, theme.primary);

            // Título de la operación
            paint.heading(title_str.c_str(), 960, 465, 30, theme.text, gfx::Align::center);

            // Barra de progreso (Track & Fill)
            const Rect track_rect{580, 505, 760, 16};
            list.rounded_rect(track_rect, 8.0f, Color::rgb(0xffffff).with_alpha(0.08f));
            paint.stroke(track_rect, 8.0f, 1.0f, Color::rgb(0xffffff).with_alpha(0.12f));

            if (progress > 0.005f)
            {
                const Rect fill_rect{track_rect.x, track_rect.y, std::max(16.0f, track_rect.w * progress), track_rect.h};
                list.rounded_rect(fill_rect, 8.0f, theme.primary);
            }

            // Porcentaje centrado
            char pct_str[32];
            std::snprintf(pct_str, sizeof(pct_str), "%d%%", pct);
            paint.heading(pct_str, 960, 565, 32, theme.primary, gfx::Align::center);

            // Detalle de transferencia (MB transferidos, archivo actual)
            if (!detail_str.empty())
            {
                paint.body(detail_str.c_str(), 960, 610, 19, theme.text.with_alpha(0.9f), gfx::Align::center);
            }

            // Advertencia de integridad y seguridad
            const char *warning_msg = is_es
                ? "Preservando Keystone & sincronizando almacenamiento. No apagues la consola."
                : "Preserving Keystone & syncing storage. Do not turn off console.";
            paint.label(warning_msg, 960, 695, 16, theme.text_muted, gfx::Align::center);

            list.pop_transform();
            list.pop_opacity();
        }

        // ==========================================
        // 5.7. DIALOGO MODAL: ALERTA / AVISO DETALLADO (Apple Liquid Glass)
        // ==========================================
        const float alert_show = alert_dialog_show_.value;
        if (alert_show > 0.005f)
        {
            list.rounded_rect({0, 0, gfx::kVirtualWidth, gfx::kVirtualHeight}, 0,
                              Color::rgb(0x000000).with_alpha(0.74f * alert_show));
            list.push_opacity(tween::clamp01(alert_show));
            list.push_transform(0.93f + 0.07f * tween::clamp01(alert_show), 960, 540, 0, 0);

            const Rect dlg{420, 180, 1080, 720};
            paint.panel(dlg);

            const bool is_es = (machete::i18n::current_language() == machete::i18n::Language::Spanish);

            // Badge / Icono de Encabezado (Rojo para error / advertencia, Verde para exito)
            const Rect badge_rect{465, 215, 48, 48};
            list.rounded_rect(badge_rect, 12.0f,
                              alert_is_error_ ? Color::rgb(0x381216).with_alpha(0.9f)
                                              : Color::rgb(0x0a2d1f).with_alpha(0.9f));
            paint.stroke(badge_rect, 12.0f, 1.5f,
                         alert_is_error_ ? Color::rgb(0xff4455).with_alpha(0.8f)
                                         : Color::rgb(0x38ef7d).with_alpha(0.8f));
            paint.heading(alert_is_error_ ? "!" : "OK",
                          badge_rect.cx(), badge_rect.cy() + 9, 22,
                          alert_is_error_ ? Color::rgb(0xff5566) : Color::rgb(0x38ef7d),
                          gfx::Align::center);

            // Titulo del Modal
            paint.heading(alert_title_.c_str(), 530, 252, 30, theme.text);

            // Caja de Contenido del Mensaje
            const Rect msg_box{465, 290, 990, 500};
            list.rounded_rect(msg_box, 14.0f, Color::rgb(0xffffff).with_alpha(0.04f));
            paint.stroke(msg_box, 14.0f, 1.0f,
                         alert_is_error_ ? Color::rgb(0xff4455).with_alpha(0.3f)
                                         : Color::rgb(0xffffff).with_alpha(0.08f));

            float line_y = msg_box.y + 36.0f;
            for (const auto &l : alert_lines_)
            {
                if (line_y > msg_box.y + msg_box.h - 26.0f)
                    break;
                paint.body(l.c_str(), msg_box.x + 24.0f, line_y, 20, theme.text.with_alpha(0.95f));
                line_y += 32.0f;
            }

            // Boton Aceptar enfocado
            const Rect btn_ok{dlg.cx() - 180, 816, 360, 54};
            ui::Look ok_look;
            ok_look.focus = 1.0f;
            paint.button(btn_ok, is_es ? "[X] Aceptar" : "[X] OK",
                         alert_is_error_ ? ui::ButtonKind::primary : ui::ButtonKind::primary,
                         ok_look);

            list.pop_transform();
            list.pop_opacity();
        }

        // ==========================================
        // 5.9. PERSONAJE ANIMADO CHIBI ESTILO ANIME (SHIN-CHAN)
        // ==========================================
        if (shinchan_opacity_ > 0.005f)
        {
            load_shinchan_textures();
            draw_shinchan_character(list, paint, 1710.0f, 905.0f, clock_,
                                    shinchan_opacity_, shinchan_state_, &shinchan_dance_textures_);
        }

        // ==========================================
        // 6. TECLAS Y CONTROLES DUALSENSE Y PIE DE PAGINA
        // ==========================================
        const ui::GlyphStyle glyphs = theme.dark ? ui::GlyphStyle::dark() : ui::GlyphStyle::light();
        const ui::Hint hints[] = {
            {ui::Button::l1, is_es ? "Pestañas" : "Tabs", ui::Button::r1},
            {ui::Button::l2, machete::i18n::tr(machete::i18n::StringId::HintTheme), ui::Button::r2},
            {ui::Button::dpad, machete::i18n::tr(machete::i18n::StringId::HintNavigate)},
            {ui::Button::cross, machete::i18n::tr(machete::i18n::StringId::LegendSelect)},
            {ui::Button::circle, machete::i18n::tr(machete::i18n::StringId::LegendBack)},
            {ui::Button::triangle, machete::i18n::tr(machete::i18n::StringId::LegendWebServer)},
            {ui::Button::square, machete::i18n::tr(machete::i18n::StringId::HintRefresh)},
            {ui::Button::left_stick, machete::i18n::tr(machete::i18n::StringId::HintLanguage)},
        };
        ui::HintLayout layout;
        layout.cy = 1018.0f;
        ui::draw_hints(list, fonts, glyphs, hints, 8, 96, false, layout);

        // Marca de agua de Versión y Build en el pie de página
        char footer_build[64];
        std::snprintf(footer_build, sizeof(footer_build), "MacheteSVG v%s (Build #%s)",
                      machete::build::kAppVersion, machete::build::kBuildNumber);
        paint.label(footer_build, 1824.0f, 1018.0f, 15, theme.text_muted.with_alpha(0.6f), gfx::Align::right);

        list.pop_transform();
        list.pop_opacity();
    }

    void build_tour()
    {
        tour_.clear();
    }

    app::Context &context_;
    std::vector<app::TourStep> tour_;
    float age_ = 0.0f;
    float clock_ = 0.0f;
    int theme_ = 0;
    int previous_theme_ = 0;
    tween::Timer switch_;

    int focus_ = 0;
    int selected_game_ = 0;
    int scroll_offset_ = 0;

    GameTab current_tab_ = GameTab::PS5;
    tween::Spring tab_indicator_;
    ShinchanState shinchan_state_{ShinchanState::Hidden};
    float shinchan_opacity_{0.0f};
    float shinchan_timer_{0.0f};

    std::array<tween::Spring, kItemCount> focus_amount_;
    std::array<ui::Pulse, kItemCount> press_;
    ui::Pulse refuse_;

    // Lista dinámica de juegos escaneados
    std::vector<machete::scanner::ScannedGame> games_;

    // Modales
    bool dialog_ = false;
    bool dialog_is_backup_ = true;
    int dialog_focus_ = 0;
    tween::Spring dialog_show_;
    tween::Spring dialog_focus_value_;
    std::vector<machete::save::BackupItem> available_backups_;

    bool web_server_dialog_ = false;
    int web_server_dialog_focus_ = 0;
    tween::Spring web_server_dialog_show_;

    // Modal de Alerta / Notificacion Detallada con Boton Aceptar
    bool alert_dialog_ = false;
    bool alert_is_error_ = false;
    std::string alert_title_;
    std::string alert_message_;
    std::vector<std::string> alert_lines_;
    tween::Spring alert_dialog_show_;

    // Cache de Codigo QR para el Servidor Web (calculo unico al cambiar URL)
    mutable std::string cached_qr_url_;
    mutable hui::ui::qr::QrMatrix cached_qr_matrix_;

    // Almacenamiento dinámico y Hotplug
    std::vector<machete::storage::StorageDevice> active_devices_;
    float storage_poll_timer_ = 0.0f;

    // Jailbreak Daemon y elevación en vivo
    machete::jailbreak::JailbreakInfo jb_info_;
    float jb_poll_timer_ = 0.0f;
    machete::threading::WorkerThread jb_thread_;
    std::atomic<bool> jb_thread_running_{false};
    std::atomic<bool> jb_needs_refresh_{false};
    std::atomic<bool> root_active_{false};
    std::mutex jb_mutex_;

    // Notificaciones
    float notification_timer_ = 0.0f;
    char notification_text_[256] = "";

    // Textura del icono oficial de MacheteSVG (/app0/sce_sys/icon0.png)
    mutable bool app_icon_attempted_{false};
    mutable unsigned int app_icon_texture_{0};

    // Texturas de los 12 fotogramas del GIF canonico de Shin-chan
    mutable bool shinchan_textures_attempted_{false};
    mutable std::array<unsigned int, 12> shinchan_dance_textures_{};

    // Operaciones Asíncronas en Segundo Plano (Worker Thread con 2MB de stack garantizados)
    enum class OperationType { None, Backup, Restore } op_type_ = OperationType::None;
    machete::threading::WorkerThread op_thread_;
    std::atomic<bool> is_operating_{false};
    std::atomic<float> operation_progress_{0.0f};
    std::atomic<bool> operation_result_ready_{false};
    mutable std::mutex op_mutex_;
    std::string operation_title_;
    std::string operation_detail_;
    machete::save::OperationResult operation_result_;

    tween::Spring op_progress_bar_;
    tween::Spring op_overlay_show_;
    float op_spinner_angle_{0.0f};
};

} // namespace

std::unique_ptr<app::Concept> make_machete(app::Context &context)
{
    return std::make_unique<Machete>(context);
}

} // namespace hui::concepts
