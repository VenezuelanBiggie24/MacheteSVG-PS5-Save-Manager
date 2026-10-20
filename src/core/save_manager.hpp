// MacheteSVG - Savegame Backup & Restore Operations
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/game_scanner.hpp"
#include "core/storage.hpp"

#include <functional>
#include <string>
#include <vector>

namespace machete::save
{

struct OperationResult
{
    bool success = false;
    std::string message;
    std::uint64_t bytes_transferred = 0;
    int files_transferred = 0;
};

struct OperationProgress
{
    float progress = 0.0f; // 0.0f a 1.0f
    std::string current_file;
    std::uint64_t bytes_transferred = 0;
    std::uint64_t total_bytes = 0;
    int files_transferred = 0;
};

using ProgressCallback = std::function<void(const OperationProgress &progress)>;

struct BackupItem
{
    std::string device_id;
    std::string device_label;
    storage::StorageType storage_type;
    std::string folder_path;
    std::string folder_name;
    std::string title_id;
    std::string title;
    std::string timestamp_str;
    std::string size_str;
    int file_count = 0;
    std::uint64_t total_bytes = 0;
    bool has_keystone = false;
    bool has_decrypted = false;
    bool has_crypto_vault = false;
};

// Verifica si la consola ya cuenta con un contenedor Save Dummy local (sdimg_*) para este juego
bool has_local_save_dummy_for_game(const scanner::ScannedGame &game);

// Sintetiza param.json y descriptores de slots de PS5 para partidas que carecen de metadatos
bool synthesize_ps5_param_json(const std::string &title_id,
                               const std::string &dst_target,
                               const std::string &meta_target,
                               const std::string &game_title,
                               std::uint32_t target_uid = 0,
                               std::uint32_t target_gid = 0);

// Respalda la partida de un juego hacia el dispositivo de almacenamiento seleccionado
// Preserva intacto el archivo keystone, archiva crypto_vault.bin y extrae archivos desencriptados (decrypted/) via PFS
OperationResult backup_game_save(const scanner::ScannedGame &game,
                                 const storage::StorageDevice &dest_device,
                                 ProgressCallback progress_cb = nullptr);

// Restaura una copia de seguridad desde cualquier carpeta hacia la ubicacion activa del juego
OperationResult restore_from_folder(const scanner::ScannedGame &game,
                                    const std::string &backup_folder,
                                    const std::string &source_label,
                                    ProgressCallback progress_cb = nullptr);

// Restaura una copia de seguridad hacia la ubicacion activa del juego en /user/home/.../savedata/
OperationResult restore_game_save(const scanner::ScannedGame &game,
                                  const storage::StorageDevice &source_device,
                                  ProgressCallback progress_cb = nullptr);

// Busca todas las copias de seguridad existentes de un juego en los dispositivos conectados
std::vector<BackupItem> find_available_backups(const scanner::ScannedGame &game,
                                              const std::vector<storage::StorageDevice> &devices);

// Restaura una copia de seguridad especifica elegida por el usuario
OperationResult restore_backup_item(const scanner::ScannedGame &game,
                                    const BackupItem &backup,
                                    ProgressCallback progress_cb = nullptr);

// Resigna y alinea directamente la partida local de PS4 activa en la consola (Motor Apollo completo):
// - Actualiza ACCOUNT_ID (con fallback sintetico offline de Apollo)
// - Actualiza PARAMS (User ID, sincronizacion de Title ID y recalculacion de HMAC-SHA256 oficial)
// - Elimina la proteccion contra copia (ATTRIBUTE copy-lock unlock)
// - Inyecta el Keystone autentico de la aplicacion instalada (o universal FPKG)
// - Sincroniza metadatos en savedata_meta/user/<TitleID>/ (param.sfo, slot_param.sfo, icon0.png)
// - Registra y purga banderas de corrupcion (is_broken=0, fake_broken=0) en savedata.db
OperationResult resign_local_ps4_save(const scanner::ScannedGame &game,
                                      ProgressCallback progress_cb = nullptr);

} // namespace machete::save

