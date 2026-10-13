// MacheteSVG - Savegame Backup & Restore Operations Implementation
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/save_manager.hpp"
#include "core/account_manager.hpp"
#include "core/i18n.hpp"
#include "core/jailbreak.hpp"
#include "core/keystone_manager.hpp"
#include "core/logger.hpp"
#include "core/samu_vault.hpp"
#include "core/sfo_resigner.hpp"
#include "cloud/google_drive.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace machete::save
{

namespace
{

std::uint32_t extract_user_id_from_path(const std::string &path)
{
    std::size_t pos = path.find("/user/home/");
    if (pos != std::string::npos)
    {
        std::size_t start = pos + 11;
        std::size_t end = path.find('/', start);
        std::string hex_str = (end == std::string::npos) ? path.substr(start) : path.substr(start, end - start);
        if (!hex_str.empty())
        {
            return static_cast<std::uint32_t>(std::strtoul(hex_str.c_str(), nullptr, 16));
        }
    }
    return 0x10000000;
}

bool ensure_directories_recursive(const std::string &path, uid_t owner_uid = 0, gid_t owner_gid = 0)
{
    if (path.empty())
        return false;

    std::string current;
    for (std::size_t i = 0; i < path.size(); ++i)
    {
        current += path[i];
        if (path[i] == '/' || i == path.size() - 1)
        {
            if (current.empty() || current == "/")
                continue;

            struct stat st{};
            if (stat(current.c_str(), &st) != 0)
            {
                if (mkdir(current.c_str(), 0777) == 0)
                {
                    (void)chmod(current.c_str(), 0777);
                    if (owner_uid != 0 || owner_gid != 0)
                    {
                        (void)chown(current.c_str(), owner_uid, owner_gid);
                    }
                }
            }
            else
            {
                // CRUCIAL: Asegurar permisos 0777 en rutas de usuario y datos para que los juegos puedan escribir
                if (current.find("/savedata") != std::string::npos ||
                    current.find("/MacheteSVG") != std::string::npos ||
                    current.find("/user/home") != std::string::npos)
                {
                    (void)chmod(current.c_str(), 0777);
                    if (owner_uid != 0 || owner_gid != 0)
                    {
                        (void)chown(current.c_str(), owner_uid, owner_gid);
                    }
                }
            }
        }
    }
    return true;
}

void resolve_path_ownership(const std::string &path, uid_t &uid, gid_t &gid)
{
    uid = 0;
    gid = 0;
    std::string cur = path;
    while (!cur.empty() && cur != "/")
    {
        struct stat st{};
        if (stat(cur.c_str(), &st) == 0)
        {
            if (st.st_uid != 0 || st.st_gid != 0)
            {
                uid = st.st_uid;
                gid = st.st_gid;
                return;
            }
        }
        std::size_t slash = cur.find_last_of('/');
        if (slash == std::string::npos || slash == 0)
            break;
        cur = cur.substr(0, slash);
    }
}

std::string sanitize_name(std::string name)
{
    const std::string invalid_chars = "\\/:*?\"<>|";
    for (char &c : name)
    {
        if (invalid_chars.find(c) != std::string::npos)
            c = '_';
    }
    return name;
}

std::uint64_t calculate_directory_size(const std::string &path)
{
    struct stat st{};
    if (stat(path.c_str(), &st) != 0)
        return 0;
    if (S_ISREG(st.st_mode))
        return static_cast<std::uint64_t>(st.st_size);

    DIR *dir = opendir(path.c_str());
    if (!dir)
        return 0;

    std::uint64_t total = 0;
    while (const dirent *entry = readdir(dir))
    {
        if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0)
            continue;
        const std::string sub_path = path + "/" + entry->d_name;
        struct stat sub_st{};
        if (stat(sub_path.c_str(), &sub_st) == 0)
        {
            if (S_ISDIR(sub_st.st_mode))
                total += calculate_directory_size(sub_path);
            else if (S_ISREG(sub_st.st_mode))
                total += static_cast<std::uint64_t>(sub_st.st_size);
        }
    }
    closedir(dir);
    return total;
}

bool copy_file_with_fsync(const std::string &src, const std::string &dst,
                          std::uint64_t &bytes_copied,
                          std::uint64_t total_bytes,
                          int files_count,
                          const ProgressCallback &progress_cb,
                          uid_t owner_uid = 0, gid_t owner_gid = 0)
{
    std::FILE *in = std::fopen(src.c_str(), "rb");
    if (!in)
    {
        MACHETE_LOG("[COPY]", "Fallo al abrir archivo de origen: %s (errno=%d: %s)",
                    src.c_str(), errno, std::strerror(errno));
        return false;
    }

    const std::string tmp_dst = dst + ".mcht_tmp";
    (void)unlink(tmp_dst.c_str());

    std::FILE *out = std::fopen(tmp_dst.c_str(), "wb");
    if (!out)
    {
        MACHETE_LOG("[COPY]", "Fallo al abrir archivo temporal para escritura: %s (errno=%d: %s)",
                    tmp_dst.c_str(), errno, std::strerror(errno));
        std::fclose(in);
        return false;
    }

    alignas(64) char buffer[64 * 1024];
    std::size_t n = 0;
    bool write_ok = true;
    std::uint64_t last_cb_bytes = bytes_copied;

    while ((n = std::fread(buffer, 1, sizeof(buffer), in)) > 0)
    {
        if (std::fwrite(buffer, 1, n, out) != n)
        {
            MACHETE_LOG("[COPY]", "Error escribiendo en destino temporal %s (errno=%d: %s)",
                        tmp_dst.c_str(), errno, std::strerror(errno));
            write_ok = false;
            break;
        }
        bytes_copied += n;
        // Notificar progreso cada 256 KB o al finalizar para no saturar el mutex del hilo principal
        if (progress_cb && (bytes_copied - last_cb_bytes >= 256 * 1024 || bytes_copied == total_bytes))
        {
            last_cb_bytes = bytes_copied;
            OperationProgress p;
            p.bytes_transferred = bytes_copied;
            p.total_bytes = total_bytes;
            p.files_transferred = files_count;
            p.progress = (total_bytes > 0)
                ? std::clamp(static_cast<float>(bytes_copied) / static_cast<float>(total_bytes), 0.0f, 0.98f)
                : 0.5f;
            const std::size_t slash = src.find_last_of('/');
            p.current_file = (slash != std::string::npos) ? src.substr(slash + 1) : src;
            progress_cb(p);
        }
    }

    std::fclose(in);

    if (std::fflush(out) != 0)
        write_ok = false;

    int fd = fileno(out);
    if (fd >= 0)
    {
        if (::fsync(fd) != 0)
        {
            MACHETE_LOG("[COPY]", "fsync fallo en %s (errno=%d: %s)", tmp_dst.c_str(), errno, std::strerror(errno));
            write_ok = false;
        }
    }

    if (std::fclose(out) != 0)
        write_ok = false;

    if (!write_ok)
    {
        (void)unlink(tmp_dst.c_str());
        return false;
    }

    // Reemplazo atómico con rename
    if (std::rename(tmp_dst.c_str(), dst.c_str()) != 0)
    {
        (void)unlink(dst.c_str());
        if (std::rename(tmp_dst.c_str(), dst.c_str()) != 0)
        {
            MACHETE_LOG("[COPY]", "rename fallo de %s a %s (errno=%d: %s)",
                        tmp_dst.c_str(), dst.c_str(), errno, std::strerror(errno));
            (void)unlink(tmp_dst.c_str());
            return false;
        }
    }

    // Otorgar permisos abiertos al destino para que Prospero y los juegos puedan leerlo libremente
    (void)chmod(dst.c_str(), 0666);
    if (owner_uid != 0 || owner_gid != 0)
    {
        (void)chown(dst.c_str(), owner_uid, owner_gid);
    }
    MACHETE_LOG("[COPY]", "Archivo completado: %s", dst.c_str());
    return true;
}

bool copy_directory_recursive(const std::string &src_dir, const std::string &dst_dir,
                              int &files_count, std::uint64_t &bytes_copied,
                              std::uint64_t total_bytes,
                              const ProgressCallback &progress_cb,
                              uid_t owner_uid = 0, gid_t owner_gid = 0)
{
    struct stat src_st{};
    if (stat(src_dir.c_str(), &src_st) == 0 && S_ISREG(src_st.st_mode))
    {
        ensure_directories_recursive(dst_dir, owner_uid, owner_gid);
        std::size_t slash = src_dir.find_last_of('/');
        std::string filename = (slash != std::string::npos) ? src_dir.substr(slash + 1) : src_dir;
        std::string dst_file = dst_dir + "/" + filename;

        if (copy_file_with_fsync(src_dir, dst_file, bytes_copied, total_bytes, files_count, progress_cb, owner_uid, owner_gid))
        {
            files_count++;
            return true;
        }
        return false;
    }

    ensure_directories_recursive(dst_dir, owner_uid, owner_gid);

    DIR *dir = opendir(src_dir.c_str());
    if (!dir)
    {
        MACHETE_LOG("[COPY]", "No se pudo abrir directorio origen: %s (errno=%d: %s)",
                    src_dir.c_str(), errno, std::strerror(errno));
        return false;
    }

    bool all_ok = true;
    while (const dirent *entry = readdir(dir))
    {
        if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0)
            continue;

        // No transferir directamente metadatos especiales en el flujo generico de partidas
        if (std::strcmp(entry->d_name, ".meta") == 0 ||
            std::strcmp(entry->d_name, "crypto_vault.bin") == 0)
            continue;

        const std::string src_path = src_dir + "/" + entry->d_name;
        const std::string dst_path = dst_dir + "/" + entry->d_name;

        struct stat st{};
        if (stat(src_path.c_str(), &st) == 0)
        {
            if (S_ISDIR(st.st_mode))
            {
                ensure_directories_recursive(dst_path, owner_uid, owner_gid);
                if (!copy_directory_recursive(src_path, dst_path, files_count, bytes_copied, total_bytes, progress_cb, owner_uid, owner_gid))
                    all_ok = false;
            }
            else
            {
                if (copy_file_with_fsync(src_path, dst_path, bytes_copied, total_bytes, files_count, progress_cb, owner_uid, owner_gid))
                {
                    files_count++;
                }
                else
                {
                    all_ok = false;
                }
            }
        }
    }

    closedir(dir);
    return all_ok;
}

void apply_permissions_recursive(const std::string &path, uid_t uid = 0, gid_t gid = 0)
{
    struct stat st{};
    if (stat(path.c_str(), &st) != 0)
        return;

    if (S_ISDIR(st.st_mode))
    {
        (void)chmod(path.c_str(), 0777);
        if (uid != 0 || gid != 0)
            (void)chown(path.c_str(), uid, gid);

        DIR *dir = opendir(path.c_str());
        if (dir)
        {
            while (const dirent *ent = readdir(dir))
            {
                if (std::strcmp(ent->d_name, ".") == 0 || std::strcmp(ent->d_name, "..") == 0)
                    continue;
                std::string child = path + "/" + ent->d_name;
                apply_permissions_recursive(child, uid, gid);
            }
            closedir(dir);
        }
    }
    else if (S_ISREG(st.st_mode))
    {
        (void)chmod(path.c_str(), 0666);
        if (uid != 0 || gid != 0)
            (void)chown(path.c_str(), uid, gid);
    }
}

void reconstruct_prospero_metadata(const std::string &title_id,
                                   const std::string &dst_target,
                                   const std::string &meta_target,
                                   uid_t target_uid, gid_t target_gid)
{
    MACHETE_LOG("[RESTORE]", "Reconstruyendo metadatos Prospero para %s en %s...",
                title_id.c_str(), meta_target.c_str());

    // 1. Copiar icon0.png del juego instalado si existe
    const std::vector<std::string> icon_candidates = {
        "/user/app/" + title_id + "/sce_sys/icon0.png",
        "/mnt/ext0/user/app/" + title_id + "/sce_sys/icon0.png",
        "/mnt/ext1/user/app/" + title_id + "/sce_sys/icon0.png",
        "/user/appmeta/" + title_id + "/icon0.png",
        "/user/appmeta/" + title_id + "/sce_sys/icon0.png",
        "/system_data/priv/appmeta/" + title_id + "/icon0.png"};

    std::string found_icon;
    for (const auto &ic : icon_candidates)
    {
        if (access(ic.c_str(), R_OK) == 0)
        {
            found_icon = ic;
            break;
        }
    }

    if (!found_icon.empty())
    {
        const std::string dst_icon = meta_target + "/icon0.png";
        std::uint64_t b = 0;
        int fc = 0;
        copy_file_with_fsync(found_icon, dst_icon, b, 0, fc, nullptr, target_uid, target_gid);
        MACHETE_LOG("[RESTORE]", "Icono reconstruido en %s (desde %s)", dst_icon.c_str(), found_icon.c_str());
    }

    // 2. Copiar param.json del juego si existe
    const std::vector<std::string> param_candidates = {
        "/user/app/" + title_id + "/sce_sys/param.json",
        "/mnt/ext0/user/app/" + title_id + "/sce_sys/param.json",
        "/mnt/ext1/user/app/" + title_id + "/sce_sys/param.json",
        "/user/appmeta/" + title_id + "/param.json",
        "/user/appmeta/" + title_id + "/sce_sys/param.json",
        "/system_data/priv/appmeta/" + title_id + "/param.json"};

    std::string found_param;
    for (const auto &pc : param_candidates)
    {
        if (access(pc.c_str(), R_OK) == 0)
        {
            found_param = pc;
            break;
        }
    }

    if (!found_param.empty())
    {
        const std::string dst_param = meta_target + "/param.json";
        std::uint64_t b = 0;
        int fc = 0;
        copy_file_with_fsync(found_param, dst_param, b, 0, fc, nullptr, target_uid, target_gid);
        MACHETE_LOG("[RESTORE]", "Parametros reconstruidos en %s (desde %s)", dst_param.c_str(), found_param.c_str());
    }

    // 3. Para cada sdimg_<slot> en dst_target, asociar icono complementario si está disponible
    DIR *dir = opendir(dst_target.c_str());
    if (dir)
    {
        while (const dirent *ent = readdir(dir))
        {
            if (std::strncmp(ent->d_name, "sdimg_", 6) == 0)
            {
                std::string slot_name = ent->d_name + 6;
                if (slot_name.rfind("sce_bu_", 0) != 0 && !found_icon.empty())
                {
                    const std::string slot_icon = meta_target + "/" + slot_name + ".png";
                    if (access(slot_icon.c_str(), F_OK) != 0)
                    {
                        std::uint64_t b = 0;
                        int fc = 0;
                        copy_file_with_fsync(found_icon, slot_icon, b, 0, fc, nullptr, target_uid, target_gid);
                    }
                }
            }
        }
        closedir(dir);
    }
}

void post_process_restored_save(const scanner::ScannedGame &game,
                                const std::string &dst_target,
                                const std::string &backup_folder,
                                uid_t target_uid, gid_t target_gid)
{
    const bool is_ps5 = (game.platform == "PS5" || game.title_id.rfind("PPSA", 0) == 0);

    if (is_ps5)
    {
        // 1. Resolver rutas de metadatos de PS5:
        // Tanto /user/home/<uid>/savedata_prospero_meta/<TitleID> como /user/home/<uid>/savedata_prospero_meta/user/<TitleID>
        std::uint32_t uid_val = extract_user_id_from_path(dst_target);
        char uid_hex[32];
        std::snprintf(uid_hex, sizeof(uid_hex), "%x", uid_val);

        const std::string meta_target_direct = std::string("/user/home/") + uid_hex + "/savedata_prospero_meta/" + game.title_id;
        const std::string meta_target_user = std::string("/user/home/") + uid_hex + "/savedata_prospero_meta/user/" + game.title_id;

        std::string backup_meta = backup_folder + "/.meta";
        if (access(backup_meta.c_str(), F_OK) != 0)
        {
            backup_meta = backup_folder + "/meta";
        }

        for (const auto &meta_target : {meta_target_direct, meta_target_user})
        {
            ensure_directories_recursive(meta_target, target_uid, target_gid);

            if (access(backup_meta.c_str(), F_OK) == 0)
            {
                int m_files = 0;
                std::uint64_t m_bytes = 0;
                (void)copy_directory_recursive(backup_meta, meta_target, m_files, m_bytes, 0, nullptr, target_uid, target_gid);
                MACHETE_LOG("[RESTORE]", "Metadatos restaurados para PS5: %d archivos en %s", m_files, meta_target.c_str());
            }
            else
            {
                // Verificar si meta_target ya tiene archivos legítimos generados por la consola (Dummy Save)
                int existing_meta_count = 0;
                DIR *dmeta = opendir(meta_target.c_str());
                if (dmeta)
                {
                    while (const dirent *me = readdir(dmeta))
                    {
                        if (me->d_name[0] != '.')
                            existing_meta_count++;
                    }
                    closedir(dmeta);
                }

                if (existing_meta_count > 0)
                {
                    MACHETE_LOG("[RESTORE]", "Metadatos existentes preservados en %s (%d archivos - Dummy Save protegido)",
                                meta_target.c_str(), existing_meta_count);
                }
                else
                {
                    reconstruct_prospero_metadata(game.title_id, dst_target, meta_target, target_uid, target_gid);
                }
            }
            apply_permissions_recursive(meta_target, target_uid, target_gid);
        }

        // Limpiar directorio residual de cloud-sync para forzar al sistema a tratar la partida local como autoritativa
        const std::string cloud_sync_dir = std::string("/user/home/") + uid_hex + "/savedata_prospero_for_cloud/" + game.title_id;
        if (access(cloud_sync_dir.c_str(), F_OK) == 0)
        {
            (void)unlink((cloud_sync_dir + "/sync.dat").c_str());
        }
    }
    else
    {
        // 2. Partidas PS4: Inyeccion de Keystone autentico y Resignado SFO para la cuenta activa
        MACHETE_LOG("[RESTORE]", "Procesando partida PS4 para cuenta actual: Keystone & param.sfo...");
        (void)machete::keystone::inject_authentic_keystone(dst_target, game.title_id, backup_folder);

        std::uint32_t user_id = extract_user_id_from_path(dst_target);
        std::uint64_t active_account_id = machete::account::detect_active_account_id(user_id);

        std::uint8_t console_psid[16] = {0};
        const bool has_psid = machete::vault::get_console_open_psid(console_psid);

        machete::sfo::ResignParams rp;
        rp.target_user_id = user_id;
        rp.target_account_id = active_account_id;
        if (has_psid)
        {
            rp.psid = console_psid;
            MACHETE_LOG("[RESTORE]", "OpenPSID obtenido de la consola fisica para recalculacion HMAC");
        }

        for (const std::string &sfo_path : {
                 dst_target + "/param.sfo",
                 dst_target + "/sce_sys/param.sfo",
                 dst_target + "/sce_sys/PARAM.SFO"})
        {
            if (access(sfo_path.c_str(), F_OK) == 0)
            {
                if (machete::sfo::resign_param_sfo(sfo_path, rp))
                {
                    MACHETE_LOG("[RESTORE]", "param.sfo resignado exitosamente con UserID=0x%08x, AccountID=0x%016llx en %s",
                                user_id, static_cast<unsigned long long>(active_account_id), sfo_path.c_str());
                }
            }
        }

        // Replicar metadatos de PS4 en savedata_meta si existen
        char uid_hex[32];
        std::snprintf(uid_hex, sizeof(uid_hex), "%x", user_id);
        const std::string meta_ps4_user = std::string("/user/home/") + uid_hex + "/savedata_meta/user/" + game.title_id;
        ensure_directories_recursive(meta_ps4_user, target_uid, target_gid);
        apply_permissions_recursive(meta_ps4_user, target_uid, target_gid);
    }

    // 3. Garantizar permisos 0777 (directorios) y 0666 (archivos) en todo el arbol restaurado
    apply_permissions_recursive(dst_target, target_uid, target_gid);
    ::sync();
}

std::string resolve_default_save_path(const scanner::ScannedGame &game)
{
    DIR *home = opendir("/user/home");
    if (!home)
        return "";

    std::string user_id;
    while (const dirent *ent = readdir(home))
    {
        if (ent->d_name[0] == '.')
            continue;
        user_id = ent->d_name;
        break; // Tomar el primer identificador de usuario activo
    }
    closedir(home);

    if (user_id.empty())
        return "";

    const bool is_ps5 = (game.platform == "PS5" || game.title_id.rfind("PPSA", 0) == 0);
    const std::string base_dir = std::string("/user/home/") + user_id + "/" +
                                 (is_ps5 ? "savedata_prospero" : "savedata");

    const std::string target = base_dir + "/" + game.title_id;
    ensure_directories_recursive(target);
    return target;
}

} // namespace

OperationResult backup_game_save(const scanner::ScannedGame &game,
                                 const storage::StorageDevice &dest_device,
                                 ProgressCallback progress_cb)
{
    OperationResult res;
    const bool is_es = (machete::i18n::current_language() == machete::i18n::Language::Spanish);

    if (dest_device.type == storage::StorageType::Cloud)
    {
        auto &drive = cloud::GoogleDrive::instance();
        drive.backup_save(game.title_id, game.title, game.save_path, nullptr);
        res.success = true;
        res.message = is_es ? ("Enviando a Google Drive /MacheteSVG/" + game.title_id + "...")
                            : ("Sending to Google Drive /MacheteSVG/" + game.title_id + "...");
        return res;
    }

    if (!jailbreak::verify_root_access(static_cast<int>(getpid())))
    {
        MACHETE_LOG("[BACKUP]", "Privilegios root ausentes (UID=%d, EUID=%d). Solicitando elevacion...",
                    static_cast<int>(getuid()), static_cast<int>(geteuid()));
        const auto jb = jailbreak::request_jailbreak();
        if (!jb.root_access)
        {
            res.success = false;
            res.message = is_es
                ? "Fallo de privilegios: se requiere jailbreak (UID 0) para acceder a los datos de guardado"
                : "Privilege failure: jailbreak (UID 0) required to access save data";
            MACHETE_LOG("[BACKUP]", "Elevacion fallida. Operacion abortada de forma segura.");
            return res;
        }
    }

    std::string root_dir = dest_device.root_dir;
    if (root_dir.empty())
    {
        if (dest_device.type == storage::StorageType::Internal)
            root_dir = "/data/" + std::string(storage::ROOT_DIR_NAME);
        else if (dest_device.type == storage::StorageType::SSD)
            root_dir = "/mnt/ext1/" + std::string(storage::ROOT_DIR_NAME);
        else if (dest_device.type == storage::StorageType::USB)
            root_dir = "/mnt/usb0/" + std::string(storage::ROOT_DIR_NAME);
    }

    if (game.save_path.empty() || access(game.save_path.c_str(), F_OK) != 0)
    {
        res.success = false;
        res.message = is_es
            ? ("No se encontraron datos de guardado en consola para " + game.title_id)
            : ("No save data found on console for " + game.title_id);
        MACHETE_LOG("[BACKUP]", "Fallo: save_path '%s' inaccesible para %s",
                    game.save_path.c_str(), game.title_id.c_str());
        return res;
    }

    // Ruta destino: <root_dir>/<title_id> - <title>/
    const std::string dest_game_folder =
        root_dir + "/" + game.title_id + " - " + sanitize_name(game.title);

    ensure_directories_recursive(root_dir);
    ensure_directories_recursive(dest_game_folder);

    std::uint64_t total_bytes = game.total_bytes;
    if (total_bytes == 0)
        total_bytes = calculate_directory_size(game.save_path);

    int files_count = 0;
    std::uint64_t bytes_copied = 0;
    MACHETE_LOG("[BACKUP]", "Iniciando respaldo de %s hacia %s",
                game.save_path.c_str(), dest_game_folder.c_str());
    const bool ok = copy_directory_recursive(game.save_path, dest_game_folder, files_count,
                                             bytes_copied, total_bytes, progress_cb);

    const bool is_ps5 = (game.platform == "PS5" || game.title_id.rfind("PPSA", 0) == 0);
    if (is_ps5)
    {
        std::string meta_path = game.save_path;
        std::size_t pos = meta_path.find("/savedata_prospero/");
        if (pos != std::string::npos)
        {
            meta_path.replace(pos, 19, "/savedata_prospero_meta/");
            if (access(meta_path.c_str(), F_OK) == 0)
            {
                int meta_files = 0;
                std::uint64_t meta_bytes = 0;
                std::string dest_meta = dest_game_folder + "/.meta";
                ensure_directories_recursive(dest_meta);
                copy_directory_recursive(meta_path, dest_meta, meta_files, meta_bytes, 0, nullptr);
                files_count += meta_files;
                bytes_copied += meta_bytes;
                MACHETE_LOG("[BACKUP]", "Metadatos respaldados para PS5: %d archivos en %s",
                            meta_files, dest_meta.c_str());
            }
        }
    }

    std::uint32_t active_user_id = extract_user_id_from_path(game.save_path);
    (void)machete::vault::archive_crypto_vault(dest_game_folder, game.title_id, game.platform,
                                               game.save_path, active_user_id);
    ::sync();

    if (progress_cb)
    {
        OperationProgress p;
        p.bytes_transferred = bytes_copied;
        p.total_bytes = total_bytes > 0 ? total_bytes : bytes_copied;
        p.files_transferred = files_count;
        p.progress = 1.0f;
        p.current_file = is_es ? "Completado" : "Completed";
        progress_cb(p);
    }

    res.success = ok && (files_count > 0);
    res.files_transferred = files_count;
    res.bytes_transferred = bytes_copied;

    if (res.success)
    {
        res.message = is_es
                          ? ("Respaldo completado (" + std::to_string(files_count) +
                             " archivos) en " + dest_game_folder)
                          : ("Backup completed (" + std::to_string(files_count) +
                             " files) at " + dest_game_folder);
        MACHETE_LOG("[BACKUP]", "Respaldo exitoso: %d archivos, %llu bytes",
                    files_count, static_cast<unsigned long long>(bytes_copied));
    }
    else
    {
        res.message = is_es
                          ? ("Aviso: Copia fallida o 0 archivos copiados. Revisa permisos de unidad.")
                          : ("Warning: Copy failed or 0 files copied. Check storage permissions.");
        MACHETE_LOG("[BACKUP]", "Fallo al respaldar %s hacia %s", game.title_id.c_str(), dest_game_folder.c_str());
    }

    return res;
}

OperationResult restore_game_save(const scanner::ScannedGame &game,
                                  const storage::StorageDevice &source_device,
                                  ProgressCallback progress_cb)
{
    OperationResult res;
    const bool is_es = (machete::i18n::current_language() == machete::i18n::Language::Spanish);

    if (!jailbreak::verify_root_access(static_cast<int>(getpid())))
    {
        MACHETE_LOG("[RESTORE]", "Privilegios root ausentes (UID=%d, EUID=%d). Solicitando elevacion...",
                    static_cast<int>(getuid()), static_cast<int>(geteuid()));
        const auto jb = jailbreak::request_jailbreak();
        if (!jb.root_access)
        {
            res.success = false;
            res.message = is_es
                ? "Fallo de privilegios: se requiere jailbreak (UID 0) para restaurar en /user/home"
                : "Privilege failure: jailbreak (UID 0) required to restore to /user/home";
            MACHETE_LOG("[RESTORE]", "Elevacion fallida. Operacion abortada de forma segura.");
            return res;
        }
    }

    if (source_device.type == storage::StorageType::Cloud)
    {
        auto &drive = cloud::GoogleDrive::instance();
        drive.restore_save(game.title_id, game.save_path, nullptr);
        res.success = true;
        res.message = is_es ? "Descargando desde Google Drive..." : "Downloading from Google Drive...";
        return res;
    }

    // Buscar carpeta de respaldo existente (soporta coincidencia exacta o con nombre del juego)
    std::string backup_game_folder =
        source_device.root_dir + "/" + game.title_id + " - " + sanitize_name(game.title);

    if (access(backup_game_folder.c_str(), F_OK) != 0)
    {
        // Probar si solo tiene el title_id
        std::string fallback_folder = source_device.root_dir + "/" + game.title_id;
        if (access(fallback_folder.c_str(), F_OK) == 0)
            backup_game_folder = fallback_folder;
    }

    if (access(backup_game_folder.c_str(), F_OK) != 0)
    {
        res.success = false;
        res.message = is_es ? ("No se encontro respaldo previo en " + source_device.label)
                            : ("No previous backup found on " + source_device.label);
        MACHETE_LOG("[RESTORE]", "No se encontro carpeta de respaldo: %s", backup_game_folder.c_str());
        return res;
    }

    const std::uint64_t total_dir_bytes = calculate_directory_size(backup_game_folder);
    if (total_dir_bytes == 0)
    {
        res.success = false;
        res.message = is_es ? "La carpeta de respaldo esta vacia (0 bytes)"
                            : "Backup folder is empty (0 bytes)";
        MACHETE_LOG("[RESTORE]", "Error: respaldo vacio en %s", backup_game_folder.c_str());
        return res;
    }

    std::string dst_target = game.save_path;
    if (dst_target.empty())
    {
        dst_target = resolve_default_save_path(game);
        MACHETE_LOG("[RESTORE]", "Ruta resuelta dinamicamente para %s: %s",
                    game.title_id.c_str(), dst_target.c_str());
    }

    if (dst_target.empty())
    {
        res.success = false;
        res.message = is_es ? "Ruta de guardado del juego no localizada en la consola"
                            : "Game save path not located on console";
        MACHETE_LOG("[RESTORE]", "No se pudo determinar destino para %s", game.title_id.c_str());
        return res;
    }

    struct stat save_st{};
    if (stat(dst_target.c_str(), &save_st) == 0 && S_ISREG(save_st.st_mode))
    {
        std::size_t slash = dst_target.find_last_of('/');
        if (slash != std::string::npos)
            dst_target = dst_target.substr(0, slash);
    }

    uid_t target_uid = 0;
    gid_t target_gid = 0;
    resolve_path_ownership(dst_target, target_uid, target_gid);
    ensure_directories_recursive(dst_target, target_uid, target_gid);

    int files_count = 0;
    std::uint64_t bytes_copied = 0;
    MACHETE_LOG("[RESTORE]", "Restaurando desde %s hacia %s (uid=%d, gid=%d)",
                backup_game_folder.c_str(), dst_target.c_str(),
                static_cast<int>(target_uid), static_cast<int>(target_gid));
    const bool ok = copy_directory_recursive(backup_game_folder, dst_target, files_count,
                                             bytes_copied, total_dir_bytes, progress_cb,
                                             target_uid, target_gid);
    post_process_restored_save(game, dst_target, backup_game_folder, target_uid, target_gid);
    ::sync();

    if (progress_cb)
    {
        OperationProgress p;
        p.bytes_transferred = bytes_copied;
        p.total_bytes = total_dir_bytes > 0 ? total_dir_bytes : bytes_copied;
        p.files_transferred = files_count;
        p.progress = 1.0f;
        p.current_file = is_es ? "Completado" : "Completed";
        progress_cb(p);
    }

    res.success = ok && (files_count > 0);
    res.files_transferred = files_count;
    res.bytes_transferred = bytes_copied;

    if (res.success)
    {
        res.message = is_es
                          ? ("Restauracion exitosa (" + std::to_string(files_count) + " archivos)")
                          : ("Restore successful (" + std::to_string(files_count) + " files)");
        MACHETE_LOG("[RESTORE]", "Restauracion completada exitosamente: %d archivos", files_count);
    }
    else
    {
        res.message = is_es ? ("Fallo al restaurar archivos en " + dst_target)
                            : ("Failed to restore files at " + dst_target);
        MACHETE_LOG("[RESTORE]", "Error al restaurar archivos en %s", dst_target.c_str());
    }

    return res;
}

std::vector<BackupItem> find_available_backups(const scanner::ScannedGame &game,
                                              const std::vector<storage::StorageDevice> &devices)
{
    std::vector<BackupItem> backups;
    const bool is_es = (machete::i18n::current_language() == machete::i18n::Language::Spanish);

    struct TargetDevice
    {
        std::string id;
        std::string label;
        storage::StorageType type;
        std::string root_dir;
    };

    std::vector<TargetDevice> targets;
    auto add_target = [&](const std::string &id, const std::string &label,
                          storage::StorageType type, const std::string &root_dir) {
        if (root_dir.empty())
            return;
        for (const auto &t : targets)
        {
            if (t.root_dir == root_dir)
                return;
        }
        if (access(root_dir.c_str(), F_OK) == 0)
        {
            targets.push_back({id, label, type, root_dir});
        }
    };

    // 1. Agregar dispositivos provistos (verificando directamente su ruta física sin depender de flags stale)
    for (const auto &dev : devices)
    {
        if (dev.type == storage::StorageType::Cloud)
            continue;
        add_target(dev.id, dev.label, dev.type, dev.root_dir);
    }

    // 2. Verificar directamente la existencia física de las rutas estándar
    // Memoria Interna (/data/MacheteSVG)
    add_target("internal", machete::i18n::tr(machete::i18n::StringId::StorageInternal),
               storage::StorageType::Internal, "/data/" + std::string(storage::ROOT_DIR_NAME));

    // SSD M.2 (/mnt/ext1/MacheteSVG y /mnt/ext0/MacheteSVG)
    add_target("ssd_ext1", machete::i18n::tr(machete::i18n::StringId::StorageM2),
               storage::StorageType::SSD, "/mnt/ext1/" + std::string(storage::ROOT_DIR_NAME));
    add_target("ssd_ext0", machete::i18n::tr(machete::i18n::StringId::StorageM2),
               storage::StorageType::SSD, "/mnt/ext0/" + std::string(storage::ROOT_DIR_NAME));

    // Puertos USB (/mnt/usb0/MacheteSVG .. /mnt/usb7/MacheteSVG)
    for (int i = 0; i < 8; ++i)
    {
        char usb_path[32];
        std::snprintf(usb_path, sizeof(usb_path), "/mnt/usb%d", i);
        char usb_root[64];
        std::snprintf(usb_root, sizeof(usb_root), "%s/%s", usb_path, storage::ROOT_DIR_NAME);
        char label_buf[64];
        std::snprintf(label_buf, sizeof(label_buf),
                      machete::i18n::tr(machete::i18n::StringId::StorageUsbFormat), i, usb_path);
        add_target("usb" + std::to_string(i), label_buf, storage::StorageType::USB, usb_root);
    }

    for (const auto &dev : targets)
    {
        DIR *dir = opendir(dev.root_dir.c_str());
        if (!dir)
            continue;

        while (const dirent *entry = readdir(dir))
        {
            if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0)
                continue;

            std::string entry_name = entry->d_name;
            if (entry_name.rfind(game.title_id, 0) != 0)
                continue;

            std::string full_path = dev.root_dir + "/" + entry_name;
            struct stat st{};
            if (stat(full_path.c_str(), &st) != 0 || !S_ISDIR(st.st_mode))
                continue;

            BackupItem item;
            item.device_id = dev.id;
            item.device_label = dev.label;
            item.storage_type = dev.type;
            item.folder_path = full_path;
            item.folder_name = entry_name;
            item.title_id = game.title_id;
            item.title = game.title;

            std::tm tm_buf{};
            std::tm *tm_info = localtime_r(&st.st_mtime, &tm_buf);
            if (tm_info)
            {
                char time_buf[64];
                std::strftime(time_buf, sizeof(time_buf), "%d/%m/%Y, %I:%M %p", tm_info);
                item.timestamp_str = time_buf;
            }
            else
            {
                item.timestamp_str = is_es ? "Fecha desconocida" : "Unknown date";
            }

            int files = 0;
            std::uint64_t total_size = 0;
            bool keystone_found = false;

            DIR *sub = opendir(full_path.c_str());
            if (sub)
            {
                while (const dirent *se = readdir(sub))
                {
                    if (std::strcmp(se->d_name, ".") == 0 || std::strcmp(se->d_name, "..") == 0)
                        continue;
                    std::string sp = full_path + "/" + se->d_name;
                    struct stat sst{};
                    if (stat(sp.c_str(), &sst) == 0)
                    {
                        if (S_ISREG(sst.st_mode))
                        {
                            files++;
                            total_size += static_cast<std::uint64_t>(sst.st_size);
                        }
                        else if (S_ISDIR(sst.st_mode))
                        {
                            DIR *sub2 = opendir(sp.c_str());
                            if (sub2)
                            {
                                while (const dirent *se2 = readdir(sub2))
                                {
                                    if (std::strcmp(se2->d_name, ".") == 0 || std::strcmp(se2->d_name, "..") == 0)
                                        continue;
                                    std::string sp2 = sp + "/" + se2->d_name;
                                    struct stat sst2{};
                                    if (stat(sp2.c_str(), &sst2) == 0 && S_ISREG(sst2.st_mode))
                                    {
                                        files++;
                                        total_size += static_cast<std::uint64_t>(sst2.st_size);
                                        if (std::strcmp(se2->d_name, "keystone") == 0)
                                            keystone_found = true;
                                    }
                                }
                                closedir(sub2);
                            }
                        }
                    }
                }
                closedir(sub);
            }

            item.file_count = files;
            item.total_bytes = total_size;
            item.has_keystone = keystone_found;

            const double mb = static_cast<double>(total_size) / (1024.0 * 1024.0);
            char size_buf[32];
            if (mb >= 1024.0)
                std::snprintf(size_buf, sizeof(size_buf), "%.1f GB", mb / 1024.0);
            else if (mb >= 0.1)
                std::snprintf(size_buf, sizeof(size_buf), "%.1f MB", mb);
            else
                std::snprintf(size_buf, sizeof(size_buf), "%llu bytes", static_cast<unsigned long long>(total_size));
            item.size_str = size_buf;

            backups.push_back(item);
        }
        closedir(dir);
    }

    return backups;
}

OperationResult restore_backup_item(const scanner::ScannedGame &game,
                                    const BackupItem &backup,
                                    ProgressCallback progress_cb)
{
    OperationResult res;
    const bool is_es = (machete::i18n::current_language() == machete::i18n::Language::Spanish);

    if (!jailbreak::verify_root_access(static_cast<int>(getpid())))
    {
        MACHETE_LOG("[RESTORE]", "Privilegios root ausentes (UID=%d, EUID=%d). Solicitando elevacion...",
                    static_cast<int>(getuid()), static_cast<int>(geteuid()));
        const auto jb = jailbreak::request_jailbreak();
        if (!jb.root_access)
        {
            res.success = false;
            res.message = is_es
                ? "Fallo de privilegios: se requiere jailbreak (UID 0) para restaurar en /user/home"
                : "Privilege failure: jailbreak (UID 0) required to restore to /user/home";
            MACHETE_LOG("[RESTORE]", "Elevacion fallida. Operacion abortada de forma segura.");
            return res;
        }
    }

    if (access(backup.folder_path.c_str(), F_OK) != 0)
    {
        res.success = false;
        res.message = is_es ? ("No se encontro la carpeta de respaldo: " + backup.folder_path)
                            : ("Backup folder not found: " + backup.folder_path);
        MACHETE_LOG("[RESTORE]", "Carpeta de respaldo no accesible: %s", backup.folder_path.c_str());
        return res;
    }

    const std::uint64_t total_dir_bytes = calculate_directory_size(backup.folder_path);
    if (total_dir_bytes == 0)
    {
        res.success = false;
        res.message = is_es ? "La carpeta de respaldo esta vacia (0 bytes)"
                            : "Backup folder is empty (0 bytes)";
        MACHETE_LOG("[RESTORE]", "Carpeta de respaldo vacia (0 bytes): %s", backup.folder_path.c_str());
        return res;
    }

    std::string dst_target = game.save_path;
    if (dst_target.empty())
    {
        dst_target = resolve_default_save_path(game);
        MACHETE_LOG("[RESTORE]", "Ruta resuelta dinamicamente para %s: %s",
                    game.title_id.c_str(), dst_target.c_str());
    }

    if (dst_target.empty())
    {
        res.success = false;
        res.message = is_es ? "Ruta de guardado del juego no localizada en la consola"
                            : "Game save path not located on console";
        MACHETE_LOG("[RESTORE]", "No se localizo destino de guardado para %s", game.title_id.c_str());
        return res;
    }

    struct stat save_st{};
    if (stat(dst_target.c_str(), &save_st) == 0 && S_ISREG(save_st.st_mode))
    {
        std::size_t slash = dst_target.find_last_of('/');
        if (slash != std::string::npos)
            dst_target = dst_target.substr(0, slash);
    }

    uid_t target_uid = 0;
    gid_t target_gid = 0;
    resolve_path_ownership(dst_target, target_uid, target_gid);
    ensure_directories_recursive(dst_target, target_uid, target_gid);

    std::uint64_t total_bytes = backup.total_bytes;
    if (total_bytes == 0)
        total_bytes = total_dir_bytes;

    int files_count = 0;
    std::uint64_t bytes_copied = 0;
    MACHETE_LOG("[RESTORE]", "Iniciando copia recursiva desde %s hacia %s (total=%llu bytes, uid=%d, gid=%d)",
                backup.folder_path.c_str(), dst_target.c_str(),
                static_cast<unsigned long long>(total_bytes),
                static_cast<int>(target_uid), static_cast<int>(target_gid));
    const bool ok = copy_directory_recursive(backup.folder_path, dst_target, files_count,
                                             bytes_copied, total_bytes, progress_cb,
                                             target_uid, target_gid);
    post_process_restored_save(game, dst_target, backup.folder_path, target_uid, target_gid);
    ::sync();

    if (progress_cb)
    {
        OperationProgress p;
        p.bytes_transferred = bytes_copied;
        p.total_bytes = total_bytes > 0 ? total_bytes : bytes_copied;
        p.files_transferred = files_count;
        p.progress = 1.0f;
        p.current_file = is_es ? "Completado" : "Completed";
        progress_cb(p);
    }

    res.success = ok && (files_count > 0);
    res.files_transferred = files_count;
    res.bytes_transferred = bytes_copied;

    if (res.success)
    {
        res.message = is_es
                          ? ("Restauracion exitosa (" + std::to_string(files_count) +
                             " archivos, " + backup.size_str + ") desde " + backup.device_label)
                          : ("Restore successful (" + std::to_string(files_count) +
                             " files, " + backup.size_str + ") from " + backup.device_label);
        MACHETE_LOG("[RESTORE]", "Restauracion exitosa: %d archivos transferidos", files_count);
    }
    else
    {
        res.message = is_es ? ("Fallo al restaurar archivos en " + dst_target)
                            : ("Failed to restore files at " + dst_target);
        MACHETE_LOG("[RESTORE]", "Fallo al restaurar archivos en %s (archivos copiados: %d)",
                    dst_target.c_str(), files_count);
    }

    return res;
}

} // namespace machete::save
