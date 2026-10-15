// MacheteSVG - Storage Management & USB Hotplug Detection
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace machete::storage
{

// El nombre estricto de la carpeta raíz para mantener todo ordenado
constexpr const char *ROOT_DIR_NAME = "MacheteSVG";

// Tipos de almacenamiento disponibles en la consola
enum class StorageType
{
    Internal, // Memoria interna de la consola (/data)
    SSD,      // Expansión interna M.2 (/mnt/ext1 o /mnt/ext0)
    USB       // Unidades extraíbles (/mnt/usb0 .. /mnt/usb7)
};

struct StorageDevice
{
    StorageType type;
    std::string id;         // "internal", "ssd_ext1", "usb0", etc.
    std::string label;      // "Memoria Interna (/data)", "USB Kingston (/mnt/usb0)", etc.
    std::string mount_path; // "/data", "/mnt/ext1", "/mnt/usb0"
    std::string root_dir;   // "/mnt/usb0/MacheteSVG"
    bool connected = false;
    std::uint64_t free_bytes = 0;
    std::uint64_t total_bytes = 0;

    std::string free_space_formatted() const;
};

// Obtiene la ruta absoluta de la carpeta MacheteSVG según el tipo de almacenamiento.
std::string get_base_path(StorageType type);

// Verifica si un dispositivo de almacenamiento específico está disponible
bool is_storage_available(StorageType type);

// Escanea dinámicamente todos los puntos de montaje de la PS5 (Interno, SSD, USB 0 a 7, Nube)
std::vector<StorageDevice> scan_storage_devices();

// Sondea si se conectó o desconectó algún pendrive/disco USB
// Retorna true si hubo un evento de inserción o extracción
bool poll_storage_changes(std::vector<StorageDevice> &out_devices, std::string &change_description);

// Se asegura de que la estructura MacheteSVG/[Title_ID - Nombre] exista en el dispositivo.
bool ensure_directory_structure(StorageType type, const std::string &title_id,
                                const std::string &game_name);

// Asegura que la carpeta base /data/MacheteSVG exista con permisos 0777
bool ensure_internal_storage();

// Asegura la estructura directamente en una ruta de montaje específica
bool ensure_directory_structure_at(const std::string &mount_path, const std::string &title_id,
                                   const std::string &game_name);

} // namespace machete::storage
