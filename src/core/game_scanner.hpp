// MacheteSVG - Real Game Discovery & Savegame Scanner
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace machete::scanner
{

struct ScannedGame
{
    std::string title;           // Nombre real del juego (ej. "Marvel's Spider-Man 2")
    std::string title_id;        // Title ID oficial (ej. "PPSA01411" o "CUSA03173")
    std::string platform;        // "PS5" o "PS4"
    std::string save_path;       // Ruta local de guardado (/user/home/.../savedata/...)
    std::string app_path;        // Ruta de instalación (/user/app/... o /mnt/ext1/user/app/...)
    std::string icon_path;       // Ruta al icono real icon0.png
    std::string size_str;        // Tamaño formateado (ej. "14.2 MB")
    std::string timestamp_str;   // Fecha y hora formateada (ej. "Hoy, 05:30 PM")
    std::uint64_t total_bytes = 0;
    bool has_keystone = false;
    unsigned int icon_texture = 0; // Identificador de textura OpenGL (0 = sin cargar)
    bool has_save_dummy = false;   // Indica si existe contenedor PFS local (sdimg_*) para inyeccion Cross-Console
    std::string save_title_id;     // Title ID del contenedor de guardado si difiere de title_id (ej. "PPSA01970")
};

// Escanea el almacenamiento de la consola (/user/home/*/savedata/, /user/app/, /data/MacheteSVG/)
// para descubrir automáticamente todos los juegos y partidas reales instaladas.
std::vector<ScannedGame> scan_installed_games_and_saves();

// Carga las imágenes icon0.png de cada juego en texturas de hardware OpenGL utilizando stb_image
void load_game_icon_textures(std::vector<ScannedGame> &games);

// Libera las texturas de iconos asignadas en la GPU
void release_game_icon_textures(std::vector<ScannedGame> &games);

// Extrae el título oficial localizado de un archivo param.json (PS5)
std::string extract_title_from_param_json(const std::string &param_path);

// Extrae el título oficial localizado de un archivo param.sfo (PS4)
std::string extract_title_from_param_sfo(const std::string &sfo_path);

// Resuelve el nombre oficial de un Title ID de PS4 si se encuentra en el catalogo interno
std::string_view resolve_known_ps4_title(std::string_view title_id) noexcept;

// Carga una textura OpenGL desde un archivo de imagen PNG
unsigned int load_texture_from_png(const std::string &path);

// Libera una textura OpenGL en la GPU
void release_texture(unsigned int &texture_id);

// Verifica si un texto contiene caracteres legibles en la fuente SDF latina/occidental
bool is_renderable_latin(const std::string &str);

// Devuelve los Title IDs de SKUs emparejados (ej. Base vs Digital Deluxe en Death Stranding)
inline std::vector<std::string> get_paired_sku_title_ids(const std::string &title_id)
{
    if (title_id == "PPSA01969") return {"PPSA01970"};
    if (title_id == "PPSA01970") return {"PPSA01969"};
    return {};
}

inline bool is_paired_sku(const std::string &a, const std::string &b)
{
    if ((a == "PPSA01969" && b == "PPSA01970") || (a == "PPSA01970" && b == "PPSA01969"))
        return true;
    return false;
}

} // namespace machete::scanner

