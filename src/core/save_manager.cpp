// MacheteSVG - Savegame Backup & Restore Operations Implementation
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/save_manager.hpp"
#include "core/account_manager.hpp"
#include "core/i18n.hpp"
#include "core/jailbreak.hpp"
#include "core/keystone_manager.hpp"
#include "core/logger.hpp"
#include "core/pfs_mount_manager.hpp"
#include "core/samu_vault.hpp"
#include "core/sfo_resigner.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace machete::save
{

namespace
{

class ScopedDir
{
public:
    ScopedDir() noexcept : dir_(nullptr) {}
    explicit ScopedDir(DIR *dir) noexcept : dir_(dir) {}
    explicit ScopedDir(const std::string &path) noexcept : dir_(::opendir(path.c_str())) {}
    explicit ScopedDir(const char *path) noexcept : dir_(path ? ::opendir(path) : nullptr) {}
    ~ScopedDir() noexcept { reset(); }

    ScopedDir(const ScopedDir &) = delete;
    ScopedDir &operator=(const ScopedDir &) = delete;

    ScopedDir(ScopedDir &&other) noexcept : dir_(other.dir_) { other.dir_ = nullptr; }
    ScopedDir &operator=(ScopedDir &&other) noexcept
    {
        if (this != &other)
        {
            reset();
            dir_ = other.dir_;
            other.dir_ = nullptr;
        }
        return *this;
    }

    DIR *get() const noexcept { return dir_; }
    bool is_valid() const noexcept { return dir_ != nullptr; }
    explicit operator bool() const noexcept { return dir_ != nullptr; }

    DIR *release() noexcept
    {
        DIR *ret = dir_;
        dir_ = nullptr;
        return ret;
    }

    void reset(DIR *new_dir = nullptr) noexcept
    {
        if (dir_)
        {
            ::closedir(dir_);
        }
        dir_ = new_dir;
    }

private:
    DIR *dir_{nullptr};
};

class ScopedFile
{
public:
    ScopedFile() noexcept : file_(nullptr) {}
    explicit ScopedFile(std::FILE *file) noexcept : file_(file) {}
    ~ScopedFile() noexcept { reset(); }

    ScopedFile(const ScopedFile &) = delete;
    ScopedFile &operator=(const ScopedFile &) = delete;

    ScopedFile(ScopedFile &&other) noexcept : file_(other.file_) { other.file_ = nullptr; }
    ScopedFile &operator=(ScopedFile &&other) noexcept
    {
        if (this != &other)
        {
            reset();
            file_ = other.file_;
            other.file_ = nullptr;
        }
        return *this;
    }

    std::FILE *get() const noexcept { return file_; }
    bool is_valid() const noexcept { return file_ != nullptr; }
    explicit operator bool() const noexcept { return file_ != nullptr; }

    std::FILE *release() noexcept
    {
        std::FILE *ret = file_;
        file_ = nullptr;
        return ret;
    }

    void reset(std::FILE *new_file = nullptr) noexcept
    {
        if (file_)
        {
            std::fclose(file_);
        }
        file_ = new_file;
    }

private:
    std::FILE *file_{nullptr};
};

constexpr int kMaxDirectoryDepth = 8;

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

__attribute__((noinline)) bool ensure_directories_recursive(const std::string &path, uid_t owner_uid = 0, gid_t owner_gid = 0) noexcept
{
    if (path.empty())
        return false;

    char buf[1024];
    const std::size_t len = (path.size() < sizeof(buf) - 1) ? path.size() : sizeof(buf) - 1;
    std::memcpy(buf, path.data(), len);
    buf[len] = '\0';

    for (std::size_t i = 1; i <= len; ++i)
    {
        if (buf[i] == '/' || buf[i] == '\0')
        {
            const char saved = buf[i];
            buf[i] = '\0';

            if (buf[0] != '\0' && (buf[0] != '/' || buf[1] != '\0'))
            {
                struct stat st{};
                if (stat(buf, &st) != 0)
                {
                    if (mkdir(buf, 0777) == 0)
                    {
                        (void)chmod(buf, 0777);
                        if (owner_uid != 0 || owner_gid != 0)
                            (void)chown(buf, owner_uid, owner_gid);
                    }
                }
                else
                {
                    // CRUCIAL: Asegurar permisos 0777 y propiedad de usuario en rutas de guardado
                    if (std::strstr(buf, "/savedata") != nullptr ||
                        std::strstr(buf, "/MacheteSVG") != nullptr ||
                        std::strstr(buf, "/user/home") != nullptr)
                    {
                        (void)chmod(buf, 0777);
                        if (owner_uid != 0 || owner_gid != 0)
                            (void)chown(buf, owner_uid, owner_gid);
                    }
                }
            }

            buf[i] = saved;
        }
    }
    return true;
}

__attribute__((noinline)) void resolve_path_ownership(const std::string &path, uid_t &uid, gid_t &gid) noexcept
{
    uid = 0;
    gid = 0;
    if (path.empty())
        return;

    char buf[1024];
    std::size_t len = (path.size() < sizeof(buf) - 1) ? path.size() : sizeof(buf) - 1;
    std::memcpy(buf, path.data(), len);
    buf[len] = '\0';

    while (len > 1)
    {
        struct stat st{};
        if (stat(buf, &st) == 0)
        {
            if (st.st_uid != 0 || st.st_gid != 0)
            {
                uid = st.st_uid;
                gid = st.st_gid;
                return;
            }
        }
        char *last_slash = std::strrchr(buf, '/');
        if (!last_slash || last_slash == buf)
            break;
        *last_slash = '\0';
        len = static_cast<std::size_t>(last_slash - buf);
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

std::uint64_t calculate_directory_size(const std::string &path, int depth = 0)
{
    if (depth >= kMaxDirectoryDepth)
    {
        return 0;
    }

    struct stat st{};
    if (stat(path.c_str(), &st) != 0)
        return 0;
    if (S_ISREG(st.st_mode))
        return static_cast<std::uint64_t>(st.st_size);
    if (!S_ISDIR(st.st_mode))
        return 0;

    ScopedDir dir(path);
    if (!dir.is_valid())
        return 0;

    std::uint64_t total = 0;
    while (const dirent *entry = readdir(dir.get()))
    {
        if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0)
            continue;
        const std::string sub_path = path + "/" + entry->d_name;
        struct stat sub_st{};
        if (stat(sub_path.c_str(), &sub_st) == 0)
        {
            if (S_ISDIR(sub_st.st_mode))
                total += calculate_directory_size(sub_path, depth + 1);
            else if (S_ISREG(sub_st.st_mode))
                total += static_cast<std::uint64_t>(sub_st.st_size);
        }
    }
    return total;
}

bool copy_file_with_fsync(const std::string &src, const std::string &dst,
                          std::uint64_t &bytes_copied,
                          std::uint64_t total_bytes,
                          int files_count,
                          const ProgressCallback &progress_cb,
                          uid_t owner_uid = 0, gid_t owner_gid = 0)
{
    ScopedFile in(std::fopen(src.c_str(), "rb"));
    if (!in.is_valid())
    {
        MACHETE_LOG("[COPY]", "Failed to open source file: %s (errno=%d: %s)",
                    src.c_str(), errno, std::strerror(errno));
        return false;
    }

    const std::string tmp_dst = dst + ".mcht_tmp";
    (void)unlink(tmp_dst.c_str());

    ScopedFile out(std::fopen(tmp_dst.c_str(), "wb"));
    if (!out.is_valid())
    {
        MACHETE_LOG("[COPY]", "Failed to open temp destination file: %s (errno=%d: %s)",
                    tmp_dst.c_str(), errno, std::strerror(errno));
        return false;
    }

    constexpr std::size_t kCopyBufferSize = 64 * 1024;
    auto buffer = std::make_unique<char[]>(kCopyBufferSize);
    std::size_t n = 0;
    bool write_ok = true;
    std::uint64_t last_cb_bytes = bytes_copied;

    while ((n = std::fread(buffer.get(), 1, kCopyBufferSize, in.get())) > 0)
    {
        if (std::fwrite(buffer.get(), 1, n, out.get()) != n)
        {
            MACHETE_LOG("[COPY]", "Write failure to temp file %s (errno=%d: %s)",
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

    if (std::ferror(in.get()))
    {
        MACHETE_LOG("[COPY]", "Read error on source file %s (errno=%d: %s)",
                    src.c_str(), errno, std::strerror(errno));
        write_ok = false;
    }

    in.reset();

    if (std::fflush(out.get()) != 0)
        write_ok = false;

    int fd = fileno(out.get());
    if (fd >= 0)
    {
        if (::fsync(fd) != 0)
        {
            MACHETE_LOG("[COPY]", "fsync failed on %s (errno=%d: %s)", tmp_dst.c_str(), errno, std::strerror(errno));
            write_ok = false;
        }
    }

    if (std::fclose(out.release()) != 0)
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
            MACHETE_LOG("[COPY]", "rename failed from %s to %s (errno=%d: %s)",
                        tmp_dst.c_str(), dst.c_str(), errno, std::strerror(errno));
            (void)unlink(tmp_dst.c_str());
            return false;
        }
    }

    // Otorgar permisos abiertos al destino para que Prospero y los juegos puedan leerlo libremente
    (void)chmod(dst.c_str(), 0666);
    if (owner_uid != 0 || owner_gid != 0)
        (void)chown(dst.c_str(), owner_uid, owner_gid);
    MACHETE_LOG("[COPY]", "File completed: %s", dst.c_str());
    return true;
}

bool copy_directory_recursive(const std::string &src_dir, const std::string &dst_dir,
                              int &files_count, std::uint64_t &bytes_copied,
                              std::uint64_t total_bytes,
                              const ProgressCallback &progress_cb,
                              uid_t owner_uid = 0, gid_t owner_gid = 0,
                              int depth = 0)
{
    if (depth >= kMaxDirectoryDepth)
    {
        MACHETE_LOG("[COPY]", "Directory depth limit exceeded (%d >= %d): %s",
                    depth, kMaxDirectoryDepth, src_dir.c_str());
        return false;
    }

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

    ScopedDir dir(src_dir);
    if (!dir.is_valid())
    {
        MACHETE_LOG("[COPY]", "Failed to open source directory: %s (errno=%d: %s)",
                    src_dir.c_str(), errno, std::strerror(errno));
        return false;
    }

    bool all_ok = true;
    while (const dirent *entry = readdir(dir.get()))
    {
        if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0)
            continue;

        // No transferir directamente metadatos especiales ni la carpeta decrypted/ en el flujo generico de contenedores
        if (std::strcmp(entry->d_name, ".meta") == 0 ||
            std::strcmp(entry->d_name, "crypto_vault.bin") == 0 ||
            (depth == 0 && std::strcmp(entry->d_name, "decrypted") == 0))
            continue;

        const std::string src_path = src_dir + "/" + entry->d_name;
        const std::string dst_path = dst_dir + "/" + entry->d_name;

        struct stat st{};
        if (stat(src_path.c_str(), &st) == 0)
        {
            if (S_ISDIR(st.st_mode))
            {
                ensure_directories_recursive(dst_path, owner_uid, owner_gid);
                if (!copy_directory_recursive(src_path, dst_path, files_count, bytes_copied, total_bytes, progress_cb, owner_uid, owner_gid, depth + 1))
                    all_ok = false;
            }
            else
            {
                MACHETE_LOG("[COPY]", "Copying file: %s -> %s", src_path.c_str(), dst_path.c_str());
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

    return all_ok;
}

void apply_permissions_recursive(const std::string &path, uid_t uid = 0, gid_t gid = 0, int depth = 0)
{
    if (depth >= kMaxDirectoryDepth)
        return;

    struct stat st{};
    if (stat(path.c_str(), &st) != 0)
        return;

    if (S_ISDIR(st.st_mode))
    {
        (void)chmod(path.c_str(), 0777);
        if (uid != 0 || gid != 0)
            (void)chown(path.c_str(), uid, gid);

        ScopedDir dir(path);
        if (dir.is_valid())
        {
            while (const dirent *ent = readdir(dir.get()))
            {
                if (std::strcmp(ent->d_name, ".") == 0 || std::strcmp(ent->d_name, "..") == 0)
                    continue;
                std::string child = path + "/" + ent->d_name;
                apply_permissions_recursive(child, uid, gid, depth + 1);
            }
        }
    }
    else if (S_ISREG(st.st_mode))
    {
        (void)chmod(path.c_str(), 0666);
        if (uid != 0 || gid != 0)
            (void)chown(path.c_str(), uid, gid);
    }
}


inline bool write_string_atomic(const std::string &path, const std::string &content, uid_t uid = 0, gid_t gid = 0)
{
    const std::string tmp = path + ".tmp";
    std::FILE *out = std::fopen(tmp.c_str(), "wb");
    if (!out)
        return false;
    std::fwrite(content.data(), 1, content.size(), out);
    std::fflush(out);
    int fd = fileno(out);
    if (fd >= 0)
        (void)::fsync(fd);
    std::fclose(out);
    (void)unlink(path.c_str());
    if (std::rename(tmp.c_str(), path.c_str()) != 0)
    {
        (void)unlink(tmp.c_str());
        return false;
    }
    (void)chmod(path.c_str(), 0666);
    if (uid != 0 || gid != 0)
        (void)chown(path.c_str(), uid, gid);
    return true;
}

} // namespace

bool synthesize_ps5_param_json(const std::string &title_id,
                               const std::string &dst_target,
                               const std::string &meta_target,
                               const std::string &game_title,
                               std::uint32_t target_uid,
                               std::uint32_t target_gid)
{
    std::vector<std::pair<std::string, std::uint64_t>> slots;
    ScopedDir dir(dst_target);
    if (dir.is_valid())
    {
        while (const dirent *ent = readdir(dir.get()))
        {
            if (std::strncmp(ent->d_name, "sdimg_", 6) == 0 && std::strncmp(ent->d_name, "sdimg_sce_bu_", 13) != 0)
            {
                std::string sname = ent->d_name + 6;
                struct stat st_sd{};
                if (stat((dst_target + "/" + ent->d_name).c_str(), &st_sd) == 0 && st_sd.st_size > 0)
                {
                    slots.push_back({sname, static_cast<std::uint64_t>(st_sd.st_size)});
                }
            }
        }
    }

    if (slots.empty())
    {
        slots.push_back({"PROFILE", 3145728ULL});
    }

    // Identificar slot primario:
    // En juegos de PS5, "PROFILE" suele ser unicamente configuracion/opciones y NO contiene el progreso de campana.
    // El slot primario debe ser la ranura de juego real (ej. AUTOSAVE0, SAVEDATA0, o la ranura que no sea PROFILE con mayor tamano).
    std::string primary_slot;
    std::uint64_t primary_size = 0;

    const char *kGameplayPrefixes[] = {
        "AUTOSAVE", "SAVEDATA", "QUICKSAVE", "MANUAL", "SAVE", "GAME", "SLOT", nullptr
    };
    for (int p = 0; kGameplayPrefixes[p] && primary_slot.empty(); ++p)
    {
        for (const auto &sl : slots)
        {
            if (sl.first.rfind(kGameplayPrefixes[p], 0) == 0)
            {
                primary_slot = sl.first;
                primary_size = sl.second;
                break;
            }
        }
    }

    if (primary_slot.empty())
    {
        for (const auto &sl : slots)
        {
            if (sl.first != "PROFILE" && sl.second > primary_size)
            {
                primary_slot = sl.first;
                primary_size = sl.second;
            }
        }
    }

    if (primary_slot.empty() && !slots.empty())
    {
        primary_slot = slots[0].first;
        primary_size = slots[0].second;
    }

    std::string safe_title = game_title.empty() ? "Save Data" : game_title;
    std::string esc_title;
    for (char c : safe_title)
    {
        if (c == '"')
            esc_title += "\\\"";
        else if (c == '\\')
            esc_title += "\\\\";
        else if (static_cast<unsigned char>(c) >= 32)
            esc_title += c;
    }
    if (esc_title.empty())
        esc_title = title_id;

    const std::uint64_t now_ms = static_cast<std::uint64_t>(std::time(nullptr)) * 1000ULL;

    char primary_json[2048];
    std::snprintf(primary_json, sizeof(primary_json),
                  "{\n"
                  "  \"titleId\": \"%s\",\n"
                  "  \"dirName\": \"%s\",\n"
                  "  \"fileSize\": %llu,\n"
                  "  \"mtime\": %llu,\n"
                  "  \"titleName\": \"%s\",\n"
                  "  \"subTitle\": \"\",\n"
                  "  \"detail\": \"\",\n"
                  "  \"userParam\": 0,\n"
                  "  \"localizedParameters\": {\n"
                  "    \"defaultLanguage\": \"es\",\n"
                  "    \"es\": {\n"
                  "      \"titleName\": \"%s\"\n"
                  "    },\n"
                  "    \"en\": {\n"
                  "      \"titleName\": \"%s\"\n"
                  "    }\n"
                  "  }\n"
                  "}\n",
                  title_id.c_str(), primary_slot.c_str(),
                  static_cast<unsigned long long>(primary_size),
                  static_cast<unsigned long long>(now_ms),
                  esc_title.c_str(), esc_title.c_str(), esc_title.c_str());

    ensure_directories_recursive(meta_target, target_uid, target_gid);

    write_string_atomic(meta_target + "/param.json", primary_json, target_uid, target_gid);

    // Escribir descriptores individuales <slot>_param.json y <slot>.json para cada slot en meta_target
    for (const auto &sl : slots)
    {
        char slot_json[1024];
        std::snprintf(slot_json, sizeof(slot_json),
                      "{\n"
                      "  \"titleId\": \"%s\",\n"
                      "  \"dirName\": \"%s\",\n"
                      "  \"fileSize\": %llu,\n"
                      "  \"mtime\": %llu,\n"
                      "  \"titleName\": \"%s\",\n"
                      "  \"subTitle\": \"%s\",\n"
                      "  \"detail\": \"\",\n"
                      "  \"userParam\": 0\n"
                      "}\n",
                      title_id.c_str(), sl.first.c_str(),
                      static_cast<unsigned long long>(sl.second),
                      static_cast<unsigned long long>(now_ms),
                      esc_title.c_str(), sl.first.c_str());

        write_string_atomic(meta_target + "/" + sl.first + "_param.json", slot_json, target_uid, target_gid);
        write_string_atomic(meta_target + "/" + sl.first + ".json", slot_json, target_uid, target_gid);
    }

    MACHETE_LOG("[RESTORE]", "Synthesized authentic PS5 param.json for %s (primary=%s, size=%llu, slots=%zu) in %s",
                title_id.c_str(), primary_slot.c_str(), static_cast<unsigned long long>(primary_size),
                slots.size(), meta_target.c_str());
    return true;
}

namespace
{

void reconstruct_prospero_metadata(const std::string &title_id,
                                   const std::string &dst_target,
                                   const std::string &meta_target,
                                   const std::string &backup_folder,
                                   const std::string &preserved_dummy_dir,
                                   const std::string &game_title,
                                   uid_t target_uid, gid_t target_gid)
{
    MACHETE_LOG("[RESTORE]", "Reconstructing Prospero metadata for %s in %s...",
                title_id.c_str(), meta_target.c_str());

    ensure_directories_recursive(meta_target, target_uid, target_gid);

    // 1. Localizar y sincronizar param.json en meta_target
    std::string found_param_json;
    const std::vector<std::string> json_candidates = {
        backup_folder + "/.meta/param.json",
        backup_folder + "/.meta/sce_sys/param.json",
        backup_folder + "/meta/param.json",
        backup_folder + "/meta/sce_sys/param.json",
        backup_folder + "/sce_sys/param.json",
        backup_folder + "/param.json",
        meta_target + "/param.json",
        preserved_dummy_dir + "/.meta_dummy/param.json",
        preserved_dummy_dir + "/param.json"
    };

    for (const auto &jc : json_candidates)
    {
        if (!jc.empty() && access(jc.c_str(), R_OK) == 0)
        {
            found_param_json = jc;
            break;
        }
    }

    if (!found_param_json.empty())
    {
        if (found_param_json != meta_target + "/param.json")
        {
            std::uint64_t b = 0;
            int fc = 0;
            copy_file_with_fsync(found_param_json, meta_target + "/param.json", b, 0, fc, nullptr, target_uid, target_gid);
        }

        // 1. Identificar el slot primario para param.json:
        // Si found_param_json ya contiene un dirName valido de este juego, PRESERVARLO estrictamente.
        // Solo si dirName esta vacio o apunta a un slot inexistente, determinar la ranura primaria mediante escaneo.
        std::string primary_slot_file;
        std::string primary_slot_name;
        struct stat st_sd{};

        // Leer dirName previo de param.json si existiera
        std::string existing_dirname;
        {
            std::FILE *jf = std::fopen((meta_target + "/param.json").c_str(), "rb");
            if (jf)
            {
                std::fseek(jf, 0, SEEK_END);
                long jsz = std::ftell(jf);
                std::fseek(jf, 0, SEEK_SET);
                if (jsz > 0 && jsz < 100000)
                {
                    std::string jcontent(static_cast<std::size_t>(jsz), '\0');
                    if (std::fread(&jcontent[0], 1, jcontent.size(), jf) == jcontent.size())
                    {
                        std::size_t dpos = jcontent.find("\"dirName\"");
                        if (dpos != std::string::npos)
                        {
                            std::size_t q1 = jcontent.find('"', dpos + 9);
                            if (q1 != std::string::npos)
                            {
                                std::size_t q2 = jcontent.find('"', q1 + 1);
                                if (q2 != std::string::npos)
                                {
                                    existing_dirname = jcontent.substr(q1 + 1, q2 - (q1 + 1));
                                }
                            }
                        }
                    }
                }
                std::fclose(jf);
            }
        }

        if (!existing_dirname.empty())
        {
            const std::string cand_existing = dst_target + "/sdimg_" + existing_dirname;
            if (stat(cand_existing.c_str(), &st_sd) == 0 && st_sd.st_size > 0)
            {
                primary_slot_file = cand_existing;
                primary_slot_name = existing_dirname;
            }
        }

        if (primary_slot_file.empty())
        {
            const char *kGameplayCandidates[] = {
                "AUTOSAVE0", "SAVEDATA0", "SAVEDATA00", "QUICKSAVE0", "MANUAL0", nullptr
            };
            for (int p = 0; kGameplayCandidates[p] && primary_slot_file.empty(); ++p)
            {
                const std::string cand_path = dst_target + "/sdimg_" + kGameplayCandidates[p];
                if (stat(cand_path.c_str(), &st_sd) == 0 && st_sd.st_size > 0)
                {
                    primary_slot_file = cand_path;
                    primary_slot_name = kGameplayCandidates[p];
                    break;
                }
            }
        }
        if (primary_slot_file.empty())
        {
            std::uint64_t max_non_profile_sz = 0;
            ScopedDir dir_j(dst_target);
            if (dir_j.is_valid())
            {
                while (const dirent *ent = readdir(dir_j.get()))
                {
                    if (std::strncmp(ent->d_name, "sdimg_", 6) == 0 && std::strncmp(ent->d_name, "sdimg_sce_bu_", 13) != 0)
                    {
                        const std::string sname = ent->d_name + 6;
                        if (sname != "PROFILE")
                        {
                            struct stat st_ent{};
                            if (stat((dst_target + "/" + ent->d_name).c_str(), &st_ent) == 0 && st_ent.st_size > 0)
                            {
                                if (static_cast<std::uint64_t>(st_ent.st_size) > max_non_profile_sz)
                                {
                                    max_non_profile_sz = static_cast<std::uint64_t>(st_ent.st_size);
                                    primary_slot_file = dst_target + "/" + ent->d_name;
                                    primary_slot_name = sname;
                                    st_sd = st_ent;
                                }
                            }
                        }
                    }
                }
            }
        }
        if (primary_slot_file.empty())
        {
            primary_slot_file = dst_target + "/sdimg_PROFILE";
            primary_slot_name = "PROFILE";
            (void)stat(primary_slot_file.c_str(), &st_sd);
        }

        if (st_sd.st_size > 0)
        {
            machete::pfs::update_ps5_param_json_dir_name(meta_target + "/param.json", primary_slot_name);
            machete::pfs::update_ps5_param_json_size(meta_target + "/param.json", static_cast<std::uint64_t>(st_sd.st_size));
        }

        // 2. Actualizar titleId dentro de param.json para coincidir exactamente con este SKU (fundamental en SKUs emparejados)
        machete::pfs::update_ps5_param_json_title_id(meta_target + "/param.json", title_id);

        // 3. Actualizar o crear descriptores tanto planos (<slot>_param.json) como jerarquicos (<slot>/param.json)
        ScopedDir dir_slots(dst_target);
        if (dir_slots.is_valid())
        {
            while (const dirent *ent = readdir(dir_slots.get()))
            {
                if (std::strncmp(ent->d_name, "sdimg_", 6) == 0 && std::strncmp(ent->d_name, "sdimg_sce_bu_", 13) != 0)
                {
                    const std::string slot_name = ent->d_name + 6;
                    struct stat st_slot{};
                    if (stat((dst_target + "/" + ent->d_name).c_str(), &st_slot) == 0 && st_slot.st_size > 0)
                    {
                        // Subcarpeta jerarquica para la ranura: meta_target/<slot_name>/
                        const std::string slot_subfolder = meta_target + "/" + slot_name;
                        ensure_directories_recursive(slot_subfolder, target_uid, target_gid);

                        const std::vector<std::string> slot_json_paths = {
                            meta_target + "/" + slot_name + "_param.json",
                            meta_target + "/" + slot_name + ".json",
                            slot_subfolder + "/param.json"
                        };
                        for (const auto &sjp : slot_json_paths)
                        {
                            if (access(sjp.c_str(), R_OK) == 0)
                            {
                                machete::pfs::update_ps5_param_json_size(sjp, static_cast<std::uint64_t>(st_slot.st_size));
                                machete::pfs::update_ps5_param_json_title_id(sjp, title_id);
                            }
                            else
                            {
                                char s_json[1024];
                                const std::uint64_t n_ms = static_cast<std::uint64_t>(std::time(nullptr)) * 1000ULL;
                                std::snprintf(s_json, sizeof(s_json),
                                              "{\n"
                                              "  \"titleId\": \"%s\",\n"
                                              "  \"dirName\": \"%s\",\n"
                                              "  \"fileSize\": %llu,\n"
                                              "  \"mtime\": %llu,\n"
                                              "  \"titleName\": \"%s\",\n"
                                              "  \"subTitle\": \"%s\",\n"
                                              "  \"detail\": \"\",\n"
                                              "  \"userParam\": 0\n"
                                              "}\n",
                                              title_id.c_str(), slot_name.c_str(),
                                              static_cast<unsigned long long>(st_slot.st_size),
                                              static_cast<unsigned long long>(n_ms),
                                              game_title.c_str(), slot_name.c_str());
                                write_string_atomic(sjp, s_json, target_uid, target_gid);
                            }
                            (void)chmod(sjp.c_str(), 0666);
                            if (target_uid != 0 || target_gid != 0)
                                (void)chown(sjp.c_str(), target_uid, target_gid);
                        }

                        // Sincronizar tambien icon0.png dentro de la subcarpeta jerarquica
                        if (access((meta_target + "/icon0.png").c_str(), R_OK) == 0)
                        {
                            (void)pfs::copy_file_atomic(meta_target + "/icon0.png", slot_subfolder + "/icon0.png", target_uid, target_gid);
                            (void)chmod((slot_subfolder + "/icon0.png").c_str(), 0666);
                        }
                    }
                }
            }
        }
        (void)chmod((meta_target + "/param.json").c_str(), 0666);
        if (target_uid != 0 || target_gid != 0)
        {
            (void)chown((meta_target + "/param.json").c_str(), target_uid, target_gid);
        }
        MACHETE_LOG("[RESTORE]", "Preserved and synchronized param.json for PS5: %s (dirName=%s)",
                    found_param_json.c_str(), primary_slot_name.c_str());
    }
    else
    {
        // Sintetizar param.json oficial de PS5 si el respaldo no incluye archivo param.json
        synthesize_ps5_param_json(title_id, dst_target, meta_target, game_title, target_uid, target_gid);
    }

    // 2. Icono de guardado: Sincronizar exclusivamente en meta_target
    std::string found_icon;
    std::vector<std::string> icon_candidates = {
        backup_folder + "/.meta/icon0.png",
        backup_folder + "/meta/icon0.png",
        backup_folder + "/icon0.png",
        backup_folder + "/sce_sys/icon0.png",
        preserved_dummy_dir + "/.meta_dummy/icon0.png",
        preserved_dummy_dir + "/sce_sys/icon0.png",
        meta_target + "/icon0.png",
        "/user/app/" + title_id + "/sce_sys/icon0.png",
        "/mnt/ext0/user/app/" + title_id + "/sce_sys/icon0.png",
        "/mnt/ext1/user/app/" + title_id + "/sce_sys/icon0.png",
        "/user/appmeta/" + title_id + "/icon0.png",
        "/user/appmeta/" + title_id + "/sce_sys/icon0.png",
        "/system_data/priv/appmeta/" + title_id + "/icon0.png"
    };

    for (const auto &ptid : scanner::get_paired_sku_title_ids(title_id))
    {
        icon_candidates.push_back("/user/appmeta/" + ptid + "/icon0.png");
        icon_candidates.push_back("/user/appmeta/" + ptid + "/sce_sys/icon0.png");
        icon_candidates.push_back("/user/app/" + ptid + "/sce_sys/icon0.png");
        icon_candidates.push_back("/system_data/priv/appmeta/" + ptid + "/icon0.png");
    }

    for (const auto &ic : icon_candidates)
    {
        if (!ic.empty() && access(ic.c_str(), R_OK) == 0)
        {
            found_icon = ic;
            break;
        }
    }

    if (!found_icon.empty())
    {
        if (found_icon != meta_target + "/icon0.png")
        {
            std::uint64_t b = 0;
            int fc = 0;
            copy_file_with_fsync(found_icon, meta_target + "/icon0.png", b, 0, fc, nullptr, target_uid, target_gid);
        }
        MACHETE_LOG("[RESTORE]", "Save icon synchronized in %s (from %s)",
                    meta_target.c_str(), found_icon.c_str());
    }

    // 3. Para cada sdimg_<slot> en dst_target, asociar <slot>.png y <slot>_icon0.png en savedata_prospero_meta
    ScopedDir dir(dst_target);
    if (dir.is_valid())
    {
        while (const dirent *ent = readdir(dir.get()))
        {
            if (std::strncmp(ent->d_name, "sdimg_", 6) == 0)
            {
                std::string slot_name = ent->d_name + 6;
                if (slot_name.rfind("sce_bu_", 0) != 0 && !found_icon.empty())
                {
                    for (const std::string &target_png : {
                             meta_target + "/" + slot_name + ".png",
                             meta_target + "/" + slot_name + "_icon0.png"})
                    {
                        if (access(target_png.c_str(), F_OK) != 0)
                        {
                            std::uint64_t b = 0;
                            int fc = 0;
                            copy_file_with_fsync(found_icon, target_png, b, 0, fc, nullptr, target_uid, target_gid);
                        }
                    }
                }
            }
        }
    }

    // 4. Sincronizar keystone autentico de 96 bytes en dst_target/keystone
    std::string found_ks;
    for (const std::string &kc : {
             dst_target + "/keystone",
             backup_folder + "/keystone",
             backup_folder + "/sce_sys/keystone",
             backup_folder + "/.meta/keystone",
             preserved_dummy_dir + "/keystone",
             preserved_dummy_dir + "/sce_sys/keystone",
             preserved_dummy_dir + "/.meta_dummy/keystone",
             "/data/MacheteSVG/keystones/" + title_id + ".keystone"})
    {
        if (access(kc.c_str(), R_OK) == 0 && machete::keystone::is_valid_keystone_file(kc))
        {
            found_ks = kc;
            break;
        }
    }
    if (!found_ks.empty())
    {
        if (found_ks != dst_target + "/keystone")
        {
            (void)pfs::copy_file_atomic(found_ks, dst_target + "/keystone", target_uid, target_gid);
        }
        (void)chmod((dst_target + "/keystone").c_str(), 0666);
        MACHETE_LOG("[RESTORE]", "Keystone synchronized in %s/keystone (from %s)",
                    dst_target.c_str(), found_ks.c_str());
    }

    // 5. PURGA ESTRICTA DE dst_target (/user/home/<uid>/savedata_prospero/<TitleID>/):
    // El sistema operativo Prospero exige que esta carpeta contenga EXCLUSIVAMENTE
    // sdimg_* y keystone. Cualquier param.json, icon0.png, *.png o sce_sys/ provoca que el
    // daemon sceSaveData marque la carpeta como datos dañados!
    ScopedDir dclean(dst_target);
    if (dclean.is_valid())
    {
        while (const dirent *ent = readdir(dclean.get()))
        {
            if (std::strcmp(ent->d_name, ".") == 0 || std::strcmp(ent->d_name, "..") == 0)
                continue;
            if (std::strncmp(ent->d_name, "sdimg_", 6) == 0 || std::strcmp(ent->d_name, "keystone") == 0)
                continue;
            const std::string foreign_item = dst_target + "/" + ent->d_name;
            struct stat st_f{};
            if (lstat(foreign_item.c_str(), &st_f) == 0)
            {
                if (S_ISDIR(st_f.st_mode))
                    pfs::remove_dir_recursive_simple(foreign_item);
                else
                    (void)unlink(foreign_item.c_str());
                MACHETE_LOG("[RESTORE]", "Purged foreign non-container file from savedata_prospero: %s", ent->d_name);
            }
        }
    }
}

void resign_sfo_directory_recursive(const std::string &dir_path,
                                    const machete::sfo::ResignParams &rp,
                                    bool &any_resigned,
                                    int depth = 0)
{
    if (depth >= 4)
        return;

    ScopedDir dir(dir_path);
    if (!dir.is_valid())
        return;

    while (const dirent *ent = readdir(dir.get()))
    {
        if (std::strcmp(ent->d_name, ".") == 0 || std::strcmp(ent->d_name, "..") == 0)
            continue;
        const std::string full = dir_path + "/" + ent->d_name;
        struct stat st{};
        if (stat(full.c_str(), &st) != 0)
            continue;

        if (S_ISDIR(st.st_mode))
        {
            resign_sfo_directory_recursive(full, rp, any_resigned, depth + 1);
        }
        else if (S_ISREG(st.st_mode))
        {
            const std::string name = ent->d_name;
            if ((name.size() >= 4 &&
                 (name.substr(name.size() - 4) == ".sfo" || name.substr(name.size() - 4) == ".SFO")) ||
                name == "param.sfo" || name == "PARAM.SFO")
            {
                if (machete::sfo::resign_param_sfo(full, rp))
                {
                    any_resigned = true;
                    MACHETE_LOG("[RESTORE]", "Resigned PS4 metadata SFO: %s", full.c_str());
                }
            }
        }
    }
}

void post_process_restored_save(const scanner::ScannedGame &game,
                                const std::string &dst_target,
                                const std::string &backup_folder,
                                const std::string &preserved_dummy_dir,
                                bool had_local_dummy,
                                uid_t target_uid, gid_t target_gid,
                                pfs::PfsRestoreReport *out_report = nullptr)
{
    const bool is_ps5 = (game.platform == "PS5" || game.title_id.rfind("PPSA", 0) == 0);
    const std::uint32_t user_id = extract_user_id_from_path(dst_target);
    const std::uint64_t active_account_id = machete::account::detect_active_account_id(user_id);

    // Extraer el Title ID real de la carpeta de guardado (por ejemplo PPSA01970 cuando el juego es PPSA01969)
    std::string save_dir_tid = game.title_id;
    {
        const std::size_t last_slash = dst_target.find_last_of('/');
        if (last_slash != std::string::npos && last_slash + 1 < dst_target.size())
        {
            const std::string tail = dst_target.substr(last_slash + 1);
            if (tail.size() == 9 && (tail.rfind("PPSA", 0) == 0 || tail.rfind("CUSA", 0) == 0))
                save_dir_tid = tail;
        }
    }

    // 1. Ejecutar motor PFS: Inyeccion en Save Dummy (Cross-Console) y Resignado dentro de contenedor PFS
    pfs::PfsRestoreReport pfs_rep = machete::pfs::inject_and_resign_save(
        save_dir_tid, is_ps5, dst_target, backup_folder, preserved_dummy_dir, had_local_dummy,
        user_id, active_account_id, target_uid, target_gid);

    // Aislar y purgar cualquier residuo de bases de datos cruzadas (PS4 vs PS5)
    db::sanitize_database_architecture_isolation(user_id);

    if (is_ps5)
    {
        // 2. Limpieza de archivos temporales residuales (.tmp)
        ScopedDir ddst(dst_target);
        if (ddst.is_valid())
        {
            while (const dirent *de = readdir(ddst.get()))
            {
                const std::string name = de->d_name;
                if (name == "." || name == "..")
                    continue;
                if (name.size() > 4 && name.substr(name.size() - 4) == ".tmp")
                {
                    (void)unlink((dst_target + "/" + name).c_str());
                    MACHETE_LOG("[RESTORE]", "Cleaned extraneous temporary file in PS5 save dir: %s", name.c_str());
                }
            }
        }

        // 3. Resolver rutas de metadatos de PS5:
        //    Tanto /user/home/<uid>/savedata_prospero_meta/<TitleID> como /user/home/<uid>/savedata_prospero_meta/user/<TitleID>
        char uid_hex[32];
        std::snprintf(uid_hex, sizeof(uid_hex), "%x", user_id);

        const std::string meta_target_direct = std::string("/user/home/") + uid_hex + "/savedata_prospero_meta/" + save_dir_tid;
        const std::string meta_target_user = std::string("/user/home/") + uid_hex + "/savedata_prospero_meta/user/" + save_dir_tid;

        std::string backup_meta = backup_folder + "/.meta";
        if (access(backup_meta.c_str(), F_OK) != 0)
        {
            backup_meta = backup_folder + "/meta";
        }

        for (const auto &meta_target : {meta_target_direct, meta_target_user})
        {
            ensure_directories_recursive(meta_target, target_uid, target_gid);

            if (!pfs_rep.used_save_dummy && access(backup_meta.c_str(), F_OK) == 0)
            {
                int m_files = 0;
                std::uint64_t m_bytes = 0;
                (void)copy_directory_recursive(backup_meta, meta_target, m_files, m_bytes, 0, nullptr, target_uid, target_gid);
                MACHETE_LOG("[RESTORE]", "Metadata restored for PS5: %d files in %s", m_files, meta_target.c_str());
            }

            reconstruct_prospero_metadata(save_dir_tid, dst_target, meta_target, backup_folder, preserved_dummy_dir, game.title, target_uid, target_gid);
            apply_permissions_recursive(meta_target, target_uid, target_gid);
        }

        // 4. Asegurar que todos los slots sdimg_* presentes en dst_target esten registrados en savedata_prospero/<uid>/db/user/savedata.db
        {
            ScopedDir dslots(dst_target);
            if (dslots.is_valid())
            {
                while (const dirent *de = readdir(dslots.get()))
                {
                    const std::string sname = de->d_name;
                    if (machete::pfs::is_valid_sdimg_name(sname))
                    {
                        struct stat st_s{};
                        std::uint64_t s_size = 0;
                        if (stat((dst_target + "/" + sname).c_str(), &st_s) == 0)
                            s_size = static_cast<std::uint64_t>(st_s.st_size);
                        db::update_or_register_ps5_savedata_db(
                            user_id, active_account_id, save_dir_tid, sname, s_size,
                            game.title, save_dir_tid);
                    }
                }
            }
            db::repair_title_corruption_flags(user_id, save_dir_tid, active_account_id);
        }

        // 5. Limpiar directorio residual de cloud-sync para forzar al sistema a tratar la partida local como autoritativa
        const std::string cloud_sync_dir = std::string("/user/home/") + uid_hex + "/savedata_prospero_for_cloud/" + save_dir_tid;
        if (access(cloud_sync_dir.c_str(), F_OK) == 0)
        {
            (void)unlink((cloud_sync_dir + "/sync.dat").c_str());
            (void)unlink((cloud_sync_dir + "/version.dat").c_str());
        }

        // 6. Sincronizacion de SKUs emparejados (ej. Death Stranding Base PPSA01969 <-> Digital Deluxe PPSA01970)
        const auto paired_skus = scanner::get_paired_sku_title_ids(save_dir_tid);
        for (const auto &paired_tid : paired_skus)
        {
            MACHETE_LOG("[RESTORE]", "Mirroring restored PS5 save to paired SKU %s (from %s)...",
                        paired_tid.c_str(), save_dir_tid.c_str());

            const std::string paired_dst = std::string("/user/home/") + uid_hex + "/savedata_prospero/" + paired_tid;
            ensure_directories_recursive(paired_dst, target_uid, target_gid);

            int p_files = 0;
            std::uint64_t p_bytes = 0;
            (void)copy_directory_recursive(dst_target, paired_dst, p_files, p_bytes, 0, nullptr, target_uid, target_gid);
            apply_permissions_recursive(paired_dst, target_uid, target_gid);

            const std::string paired_meta_direct = std::string("/user/home/") + uid_hex + "/savedata_prospero_meta/" + paired_tid;
            const std::string paired_meta_user = std::string("/user/home/") + uid_hex + "/savedata_prospero_meta/user/" + paired_tid;
            for (const auto &pmeta : {paired_meta_direct, paired_meta_user})
            {
                ensure_directories_recursive(pmeta, target_uid, target_gid);
                reconstruct_prospero_metadata(paired_tid, paired_dst, pmeta, backup_folder, preserved_dummy_dir, game.title, target_uid, target_gid);
                apply_permissions_recursive(pmeta, target_uid, target_gid);
            }

            // Indexar slots en savedata.db para el SKU emparejado
            ScopedDir dpaired(paired_dst);
            if (dpaired.is_valid())
            {
                while (const dirent *pde = readdir(dpaired.get()))
                {
                    const std::string psname = pde->d_name;
                    if (machete::pfs::is_valid_sdimg_name(psname))
                    {
                        struct stat st_ps{};
                        std::uint64_t ps_size = 0;
                        if (stat((paired_dst + "/" + psname).c_str(), &st_ps) == 0)
                            ps_size = static_cast<std::uint64_t>(st_ps.st_size);
                        db::update_or_register_ps5_savedata_db(
                            user_id, active_account_id, paired_tid, psname, ps_size,
                            game.title, game.title_id);
                    }
                }
            }
            db::repair_title_corruption_flags(user_id, paired_tid, active_account_id);

            const std::string paired_cloud = std::string("/user/home/") + uid_hex + "/savedata_prospero_for_cloud/" + paired_tid;
            if (access(paired_cloud.c_str(), F_OK) == 0)
            {
                (void)unlink((paired_cloud + "/sync.dat").c_str());
                (void)unlink((paired_cloud + "/version.dat").c_str());
            }
        }
    }
    else
    {
        // 2. Partidas PS4: Inyeccion de Keystone autentico y Resignado SFO para la cuenta activa (Modo Apollo completo)
        MACHETE_LOG("[RESTORE]", "Processing PS4 save for active account: Keystone & param.sfo (Apollo Engine)...");
        const bool backup_has_keystone = (access((backup_folder + "/keystone").c_str(), R_OK) == 0 &&
                                          machete::keystone::is_valid_keystone_file(backup_folder + "/keystone")) ||
                                         (access((backup_folder + "/sce_sys/keystone").c_str(), R_OK) == 0 &&
                                          machete::keystone::is_valid_keystone_file(backup_folder + "/sce_sys/keystone"));

        const std::string preserved_ks = preserved_dummy_dir + "/keystone";
        // 1. Prioridad maxima: Si habia un Keystone preservado del Save Dummy local de esta consola
        if (access(preserved_ks.c_str(), R_OK) == 0 && machete::keystone::is_valid_keystone_file(preserved_ks))
        {
            (void)pfs::copy_file_atomic(preserved_ks, dst_target + "/keystone", target_uid, target_gid);
            (void)chmod((dst_target + "/keystone").c_str(), 0666);
            (void)pfs::ensure_dir_tree(dst_target + "/sce_sys", target_uid, target_gid);
            (void)pfs::copy_file_atomic(preserved_ks, dst_target + "/sce_sys/keystone", target_uid, target_gid);
            (void)chmod((dst_target + "/sce_sys/keystone").c_str(), 0666);
            pfs_rep.keystone_injected = true;
            MACHETE_LOG("[KEYSTONE]", "Restored authentic local console keystone from preserved dummy: %s", dst_target.c_str());
        }
        // 2. Segunda prioridad: Keystone autentico de la aplicacion instalada en la consola (app/appmeta/PKG)
        else if (machete::keystone::inject_authentic_keystone(dst_target, save_dir_tid, "", game.app_path))
        {
            pfs_rep.keystone_injected = true;
            MACHETE_LOG("[KEYSTONE]", "Injected installed game's authentic keystone for %s into %s",
                        save_dir_tid.c_str(), dst_target.c_str());
        }
        // 3. Tercera prioridad: Keystone preservado en el respaldo si la consola no tiene el juego instalado
        else if (backup_has_keystone)
        {
            pfs_rep.keystone_injected = true;
            (void)pfs::ensure_dir_tree(dst_target + "/sce_sys", target_uid, target_gid);
            if (access((dst_target + "/keystone").c_str(), R_OK) == 0)
            {
                (void)pfs::copy_file_atomic(dst_target + "/keystone", dst_target + "/sce_sys/keystone", target_uid, target_gid);
                (void)chmod((dst_target + "/sce_sys/keystone").c_str(), 0666);
            }
            MACHETE_LOG("[KEYSTONE]", "Using preserved keystone from backup: %s", backup_folder.c_str());
        }
        // 4. Cuarta prioridad: Inyectar keystone universal FPKG estilo Apollo para juegos homebrew/fake PKG
        else
        {
            if (machete::keystone::inject_fallback_fpkg_keystone(dst_target, save_dir_tid))
            {
                pfs_rep.keystone_injected = true;
                pfs_rep.fallback_keystone_used = true;
                MACHETE_LOG("[KEYSTONE]", "Notice: Injected Apollo FPKG fallback keystone for %s into %s",
                            save_dir_tid.c_str(), dst_target.c_str());
            }
        }

        if (access((dst_target + "/keystone").c_str(), R_OK) == 0)
        {
            (void)chmod((dst_target + "/keystone").c_str(), 0666);
            if (target_uid != 0 || target_gid != 0)
                (void)chown((dst_target + "/keystone").c_str(), target_uid, target_gid);
        }

        std::uint8_t console_psid[16] = {0};
        const bool has_psid = machete::vault::get_console_open_psid(console_psid);

        machete::sfo::ResignParams rp;
        rp.target_user_id = user_id;
        rp.target_account_id = active_account_id;
        rp.apply_account_id = true;
        if (has_psid)
        {
            rp.psid = console_psid;
            MACHETE_LOG("[RESTORE]", "Console OpenPSID retrieved for HMAC recalculation");
        }

        resign_sfo_directory_recursive(dst_target, rp, pfs_rep.sfo_resigned);

        // Replicar y resignar metadatos de PS4 en savedata_meta/user/<TitleID>
        char uid_hex[32];
        std::snprintf(uid_hex, sizeof(uid_hex), "%x", user_id);
        const std::string meta_ps4_user = std::string("/user/home/") + uid_hex + "/savedata_meta/user/" + save_dir_tid;
        ensure_directories_recursive(meta_ps4_user, target_uid, target_gid);

        // Registrar una vez en el log la estructura de otra carpeta savedata_meta/user/CUSA* funcional (ej. CUSA03694) para diagnostico
        {
            static bool s_logged_ps4_meta_sample = false;
            if (!s_logged_ps4_meta_sample)
            {
                s_logged_ps4_meta_sample = true;
                const std::string meta_root = std::string("/user/home/") + uid_hex + "/savedata_meta/user";
                ScopedDir mr(meta_root);
                if (mr.is_valid())
                {
                    while (const dirent *me = readdir(mr.get()))
                    {
                        const std::string mname = me->d_name;
                        if (mname.rfind("CUSA", 0) == 0 && mname != save_dir_tid)
                        {
                            const std::string sample_dir = meta_root + "/" + mname;
                            ScopedDir sd(sample_dir);
                            if (sd.is_valid())
                            {
                                std::string files_list;
                                while (const dirent *se = readdir(sd.get()))
                                {
                                    if (se->d_name[0] == '.')
                                        continue;
                                    if (!files_list.empty())
                                        files_list += ", ";
                                    files_list += se->d_name;
                                }
                                MACHETE_LOG("[RESTORE]", "PS4 savedata_meta/user/%s sample contents: [%s]",
                                            mname.c_str(), files_list.c_str());
                            }
                            break;
                        }
                    }
                }
            }
        }

        std::string backup_meta = backup_folder + "/.meta";
        if (access(backup_meta.c_str(), F_OK) != 0)
            backup_meta = backup_folder + "/meta";

        if (!pfs_rep.used_save_dummy && access(backup_meta.c_str(), F_OK) == 0)
        {
            int m_files = 0;
            std::uint64_t m_bytes = 0;
            (void)copy_directory_recursive(backup_meta, meta_ps4_user, m_files, m_bytes, 0, nullptr, target_uid, target_gid);
        }



        // Sincronizar iconos en meta_ps4_user: En PS5, savedata_meta/user/<TitleID> solo contiene
        // system0020_icon0.png e icon0.png. Cualquier param.sfo o sce_sys huerfano alli genera conflicto con el daemon de PS4 BC.
        {
            const std::vector<std::string> icon_candidates = {
                dst_target + "/sce_sys/icon0.png",
                dst_target + "/icon0.png",
                preserved_dummy_dir + "/.meta_dummy/icon0.png",
                preserved_dummy_dir + "/.meta_dummy/system0020_icon0.png",
                preserved_dummy_dir + "/.meta_dummy/sce_sys/icon0.png",
                backup_folder + "/.meta/icon0.png",
                backup_folder + "/.meta/system0020_icon0.png",
                backup_folder + "/meta/icon0.png",
                backup_folder + "/icon0.png",
                backup_folder + "/sce_sys/icon0.png",
                "/user/app/" + save_dir_tid + "/sce_sys/icon0.png",
                "/user/appmeta/" + save_dir_tid + "/icon0.png",
                "/system_data/priv/appmeta/" + save_dir_tid + "/icon0.png"
            };
            for (const auto &ic : icon_candidates)
            {
                if (!ic.empty() && access(ic.c_str(), R_OK) == 0)
                {
                    (void)pfs::copy_file_atomic(ic, meta_ps4_user + "/icon0.png", target_uid, target_gid);
                    (void)pfs::copy_file_atomic(ic, meta_ps4_user + "/system0020_icon0.png", target_uid, target_gid);
                    break;
                }
            }

            // Sincronizar y resignar param.sfo en meta_ps4_user (/user/home/<uid>/savedata_meta/user/<TitleID>/)
            // En PS4 / PS5 BC, la consola lee param.sfo de esta carpeta para el menu de almacenamiento y el juego
            const std::vector<std::string> sfo_candidates = {
                backup_folder + "/.meta/param.sfo",
                backup_folder + "/meta/param.sfo",
                backup_folder + "/param.sfo",
                backup_folder + "/sce_sys/param.sfo",
                preserved_dummy_dir + "/.meta_dummy/param.sfo",
                preserved_dummy_dir + "/param.sfo",
                preserved_dummy_dir + "/sce_sys/param.sfo",
                dst_target + "/param.sfo",
                dst_target + "/sce_sys/param.sfo"
            };

            std::string valid_sfo;
            for (const auto &cand : sfo_candidates)
            {
                if (access(cand.c_str(), R_OK) == 0 && machete::sfo::is_valid_save_param_sfo(cand))
                {
                    valid_sfo = cand;
                    break;
                }
            }

            if (!valid_sfo.empty())
            {
                (void)pfs::copy_file_atomic(valid_sfo, meta_ps4_user + "/param.sfo", target_uid, target_gid);
                (void)machete::sfo::resign_param_sfo(meta_ps4_user + "/param.sfo", rp);
                (void)chmod((meta_ps4_user + "/param.sfo").c_str(), 0666);

                // Tambien sincronizar <slot>_param.sfo para cada ranura detectada
                ScopedDir dslots(dst_target);
                if (dslots.is_valid())
                {
                    while (const dirent *de = readdir(dslots.get()))
                    {
                        const std::string sname = de->d_name;
                        if (machete::pfs::is_valid_ps4_slot_name(dst_target, sname))
                        {
                            const std::string slot_sfo = meta_ps4_user + "/" + sname + "_param.sfo";
                            (void)pfs::copy_file_atomic(valid_sfo, slot_sfo, target_uid, target_gid);
                            (void)machete::sfo::resign_param_sfo(slot_sfo, rp);
                            (void)chmod(slot_sfo.c_str(), 0666);
                        }
                    }
                }
                MACHETE_LOG("[RESTORE]", "Synchronized and resigned PS4 metadata SFO in %s (from %s)",
                            meta_ps4_user.c_str(), valid_sfo.c_str());
            }
            else
            {
                // Si el respaldo carecia de param.sfo externo y la imagen Book/savedata.bin no tenia
                // un SFO extraible, sintetizamos un descriptor param.sfo de contingencia autentico
                const std::string synth_sfo = meta_ps4_user + "/param.sfo";
                if (machete::sfo::synthesize_ps4_save_param_sfo(synth_sfo, save_dir_tid, save_dir_tid,
                                                                 game.title, "", "", user_id, active_account_id, 325, 0, nullptr))
                {
                    (void)chmod(synth_sfo.c_str(), 0666);
                    ScopedDir dslots(dst_target);
                    if (dslots.is_valid())
                    {
                        while (const dirent *de = readdir(dslots.get()))
                        {
                            const std::string sname = de->d_name;
                            if (machete::pfs::is_valid_ps4_slot_name(dst_target, sname))
                            {
                                const std::string slot_sfo = meta_ps4_user + "/" + sname + "_param.sfo";
                                (void)pfs::copy_file_atomic(synth_sfo, slot_sfo, target_uid, target_gid);
                                (void)chmod(slot_sfo.c_str(), 0666);
                            }
                        }
                    }
                    ensure_directories_recursive(dst_target + "/sce_sys", target_uid, target_gid);
                    (void)pfs::copy_file_atomic(synth_sfo, dst_target + "/sce_sys/param.sfo", target_uid, target_gid);
                    (void)chmod((dst_target + "/sce_sys/param.sfo").c_str(), 0666);
                    MACHETE_LOG("[RESTORE]", "Synthesized contingency PS4 param.sfo for %s in %s",
                                save_dir_tid.c_str(), meta_ps4_user.c_str());
                }
            }

            // Purgar subdirectorios anomalos dentro de meta_ps4_user pero PRESERVAR los archivos .sfo y .png
            (void)unlink((meta_ps4_user + "/keystone").c_str());
            (void)unlink((meta_ps4_user + "/sce_sys/param.sfo").c_str());
            (void)unlink((meta_ps4_user + "/sce_sys/icon0.png").c_str());
            (void)rmdir((meta_ps4_user + "/sce_sys").c_str());
        }

        apply_permissions_recursive(meta_ps4_user, target_uid, target_gid);

        // 3. Indexar y deduplicar en /system_data/savedata/<uid>/db/user/savedata.db
        if (!pfs_rep.used_pfs_mount)
        {
            sfo::SfoSaveMetadata fallback_meta{};
            for (const std::string &cand_sfo : {
                     dst_target + "/param.sfo",
                     dst_target + "/sce_sys/param.sfo",
                     backup_folder + "/param.sfo",
                     backup_folder + "/.meta/param.sfo"})
            {
                if (access(cand_sfo.c_str(), R_OK) == 0)
                {
                    fallback_meta = machete::sfo::read_sfo_metadata(cand_sfo);
                    if (fallback_meta.valid)
                        break;
                }
            }
            ScopedDir ddst(dst_target);
            if (ddst.is_valid())
            {
                while (const dirent *de = readdir(ddst.get()))
                {
                    const std::string sname = de->d_name;
                    if (machete::pfs::is_valid_ps4_slot_name(dst_target, sname))
                    {
                        struct stat st_s{};
                        std::uint64_t s_size = 0;
                        if (stat((dst_target + "/" + sname).c_str(), &st_s) == 0)
                            s_size = static_cast<std::uint64_t>(st_s.st_size);
                        db::update_or_register_ps4_savedata_db(
                            user_id, active_account_id, save_dir_tid, sname, s_size,
                            fallback_meta.valid ? &fallback_meta : nullptr,
                            game.title);
                    }
                }
            }
            db::repair_title_corruption_flags(user_id, save_dir_tid, active_account_id);
        }
    }

    if (out_report)
        *out_report = pfs_rep;

    // 4. Garantizar permisos 0777 (directorios) y 0666 (archivos) en todo el arbol restaurado
    apply_permissions_recursive(dst_target, target_uid, target_gid);
    ::sync();
}

struct SaveInfo
{
    std::string game_title_id;
    std::string save_title_id;
    std::string platform;
    std::string title;
};

inline SaveInfo read_save_info(const std::string &folder)
{
    SaveInfo info{};
    const std::string info_path = folder + "/.save_info";
    std::FILE *f = std::fopen(info_path.c_str(), "r");
    if (!f)
        return info;
    char line[512];
    while (std::fgets(line, sizeof(line), f))
    {
        std::string s = line;
        while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' '))
            s.pop_back();
        const std::size_t eq = s.find('=');
        if (eq == std::string::npos)
            continue;
        const std::string k = s.substr(0, eq);
        const std::string v = s.substr(eq + 1);
        if (k == "title_id" || k == "game_title_id")
            info.game_title_id = v;
        else if (k == "save_title_id")
            info.save_title_id = v;
        else if (k == "platform")
            info.platform = v;
        else if (k == "title")
            info.title = v;
    }
    std::fclose(f);
    return info;
}

inline void write_save_info(const std::string &folder, const SaveInfo &info)
{
    const std::string info_path = folder + "/.save_info";
    std::FILE *f = std::fopen(info_path.c_str(), "w");
    if (!f)
        return;
    std::fprintf(f, "title_id=%s\n", info.game_title_id.c_str());
    if (!info.save_title_id.empty() && info.save_title_id != info.game_title_id)
        std::fprintf(f, "save_title_id=%s\n", info.save_title_id.c_str());
    std::fprintf(f, "platform=%s\n", info.platform.c_str());
    std::fprintf(f, "title=%s\n", info.title.c_str());
    std::fclose(f);
    (void)chmod(info_path.c_str(), 0666);
}

std::string resolve_default_save_path(const scanner::ScannedGame &game)
{
    ScopedDir home("/user/home");
    if (!home.is_valid())
        return "";

    std::string user_id;
    while (const dirent *ent = readdir(home.get()))
    {
        if (ent->d_name[0] == '.')
            continue;
        user_id = ent->d_name;
        break; // Tomar el primer identificador de usuario activo
    }

    if (user_id.empty())
        return "";

    const bool is_ps5 = (game.platform == "PS5" || game.title_id.rfind("PPSA", 0) == 0);
    const std::string base_dir = std::string("/user/home/") + user_id + "/" +
                                 (is_ps5 ? "savedata_prospero" : "savedata");

    const std::string effective_tid = game.title_id;
    const std::string target = base_dir + "/" + effective_tid;
    ensure_directories_recursive(target);
    return target;
}

std::string resolve_existing_save_path(const scanner::ScannedGame &game)
{
    if (!game.save_path.empty() && access(game.save_path.c_str(), F_OK) == 0 &&
        calculate_directory_size(game.save_path) > 0)
    {
        return game.save_path;
    }

    const char *home_roots[] = {
        "/user/home",
        "/mnt/ext0/user/home",
        "/mnt/ext1/user/home",
    };

    std::vector<std::string> tids = {game.title_id};
    if (!game.save_title_id.empty() && game.save_title_id != game.title_id)
        tids.push_back(game.save_title_id);
    for (const auto &ptid : scanner::get_paired_sku_title_ids(game.title_id))
    {
        if (std::find(tids.begin(), tids.end(), ptid) == tids.end())
            tids.push_back(ptid);
    }

    for (const char *hr : home_roots)
    {
        ScopedDir hdir(hr);
        if (!hdir.is_valid())
            continue;
        while (const dirent *uent = readdir(hdir.get()))
        {
            if (uent->d_name[0] == '.')
                continue;
            const std::string uid_s = uent->d_name;
            for (const auto &tid : tids)
            {
                const std::vector<std::string> candidates = {
                    std::string(hr) + "/" + uid_s + "/savedata_prospero/" + tid,
                    std::string(hr) + "/" + uid_s + "/savedata_prospero/user/" + tid,
                    std::string(hr) + "/" + uid_s + "/savedata/" + tid,
                    std::string(hr) + "/" + uid_s + "/savedata/user/" + tid,
                    std::string(hr) + "/" + uid_s + "/savedata_backup/" + tid,
                    "/system_data/savedata_prospero/" + uid_s + "/" + tid,
                    "/system_data/savedata_prospero/" + uid_s + "/user/" + tid,
                    "/system_data/savedata/" + uid_s + "/" + tid,
                    "/system_data/savedata/" + uid_s + "/user/" + tid,
                };
                for (const auto &cand : candidates)
                {
                    if (access(cand.c_str(), F_OK) == 0 && calculate_directory_size(cand) > 0)
                        return cand;
                }
            }
        }
    }

    return game.save_path;
}

} // namespace

bool has_local_save_dummy_for_game(const scanner::ScannedGame &game)
{
    const bool is_ps5 = (game.platform == "PS5" || game.title_id.rfind("PPSA", 0) == 0);
    std::string target = game.save_path;
    if (!target.empty() && pfs::has_local_save_dummy(target, is_ps5))
        return true;

    std::vector<std::string> tids = {game.title_id};
    if (!game.save_title_id.empty() && game.save_title_id != game.title_id)
        tids.push_back(game.save_title_id);
    for (const auto &ptid : scanner::get_paired_sku_title_ids(game.title_id))
    {
        if (std::find(tids.begin(), tids.end(), ptid) == tids.end())
            tids.push_back(ptid);
    }

    ScopedDir home("/user/home");
    if (home.is_valid())
    {
        while (const dirent *ent = readdir(home.get()))
        {
            if (ent->d_name[0] == '.')
                continue;
            for (const auto &tid : tids)
            {
                const std::string cand = std::string("/user/home/") + ent->d_name + "/" +
                                         (is_ps5 ? "savedata_prospero/" : "savedata/") + tid;
                if (pfs::has_local_save_dummy(cand, is_ps5))
                    return true;
            }
        }
    }
    return false;
}

__attribute__((noinline)) OperationResult backup_game_save(const scanner::ScannedGame &game,
                                                              const storage::StorageDevice &dest_device,
                                                              ProgressCallback progress_cb)
{
    MACHETE_BREADCRUMB("backup_game_save: entry");
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    try
    {
#endif
        OperationResult res;
        const bool is_es = (machete::i18n::current_language() == machete::i18n::Language::Spanish);

        MACHETE_BREADCRUMB("backup_game_save: verify_root");

        if (!jailbreak::verify_root_access(static_cast<int>(getpid())))
        {
            MACHETE_LOG("[BACKUP]", "Root privileges absent (UID=%d, EUID=%d). Requesting elevation...",
                        static_cast<int>(getuid()), static_cast<int>(geteuid()));
            const auto jb = jailbreak::request_jailbreak();
            if (!jb.root_access)
            {
                res.success = false;
                res.message = is_es
                    ? "Fallo de privilegios: se requiere jailbreak (UID 0) para acceder a los datos de guardado"
                    : "Privilege failure: jailbreak (UID 0) required to access save data";
                MACHETE_LOG("[BACKUP]", "Elevation failed. Safe operation aborted.");
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

        const std::string active_save_path = resolve_existing_save_path(game);
        if (active_save_path.empty() || access(active_save_path.c_str(), F_OK) != 0)
        {
            res.success = false;
            res.message = is_es
                ? ("No se encontraron datos de guardado en consola para " + game.title_id)
                : ("No save data found on console for " + game.title_id);
            MACHETE_LOG("[BACKUP]", "Error: save_path '%s' inaccessible for %s",
                        active_save_path.c_str(), game.title_id.c_str());
            return res;
        }

        // Ruta destino: <root_dir>/<title_id> - <title>/
        const std::string dest_game_folder =
            root_dir + "/" + game.title_id + " - " + sanitize_name(game.title);

        ensure_directories_recursive(root_dir);
        ensure_directories_recursive(dest_game_folder);

        std::uint64_t total_bytes = game.total_bytes;
        if (total_bytes == 0)
            total_bytes = calculate_directory_size(active_save_path);

        int files_count = 0;
        std::uint64_t bytes_copied = 0;
        MACHETE_LOG("[BACKUP]", "Starting backup of %s -> %s",
                    active_save_path.c_str(), dest_game_folder.c_str());
        const bool ok = copy_directory_recursive(active_save_path, dest_game_folder, files_count,
                                                 bytes_copied, total_bytes, progress_cb);

        const bool is_ps5 = (game.platform == "PS5" || game.title_id.rfind("PPSA", 0) == 0);
        if (is_ps5)
        {
            std::string meta_path = active_save_path;
            std::size_t pos = meta_path.find("/savedata_prospero/");
            if (pos != std::string::npos)
            {
                std::string meta_direct = meta_path;
                meta_direct.replace(pos, 19, "/savedata_prospero_meta/");
                std::string meta_user = meta_path;
                meta_user.replace(pos, 19, "/savedata_prospero_meta/user/");
                const std::string cand_meta = (access(meta_user.c_str(), F_OK) == 0) ? meta_user
                                            : ((access(meta_direct.c_str(), F_OK) == 0) ? meta_direct : "");
                if (!cand_meta.empty())
                {
                    int meta_files = 0;
                    std::uint64_t meta_bytes = 0;
                    std::string dest_meta = dest_game_folder + "/.meta";
                    std::string dest_meta_vis = dest_game_folder + "/meta";
                    ensure_directories_recursive(dest_meta);
                    ensure_directories_recursive(dest_meta_vis);
                    copy_directory_recursive(cand_meta, dest_meta, meta_files, meta_bytes, 0, nullptr);
                    int vis_files = 0;
                    std::uint64_t vis_bytes = 0;
                    copy_directory_recursive(cand_meta, dest_meta_vis, vis_files, vis_bytes, 0, nullptr);
                    files_count += meta_files;
                    bytes_copied += meta_bytes;
                    MACHETE_LOG("[BACKUP]", "Metadata backed up for PS5: %d files from %s to %s and %s",
                                meta_files, cand_meta.c_str(), dest_meta.c_str(), dest_meta_vis.c_str());
                }
            }

            // Si el respaldo no contiene param.json, sintetizarlo a partir de los slots sdimg_* respaldados
            if (access((dest_game_folder + "/.meta/param.json").c_str(), F_OK) != 0 &&
                access((dest_game_folder + "/param.json").c_str(), F_OK) != 0)
            {
                std::string meta_dir = dest_game_folder + "/.meta";
                ensure_directories_recursive(meta_dir);
                ensure_directories_recursive(dest_game_folder + "/meta");
                synthesize_ps5_param_json(game.title_id, dest_game_folder, meta_dir, game.title, 0, 0);
                if (access((meta_dir + "/param.json").c_str(), F_OK) == 0 &&
                    access((dest_game_folder + "/meta/param.json").c_str(), F_OK) != 0)
                {
                    (void)pfs::copy_file_atomic(meta_dir + "/param.json", dest_game_folder + "/meta/param.json", 0, 0);
                }
            }

            // Asegurar icon0.png en el respaldo (.meta, meta y raiz)
            if (access((dest_game_folder + "/.meta/icon0.png").c_str(), F_OK) != 0 &&
                access((dest_game_folder + "/icon0.png").c_str(), F_OK) != 0)
            {
                std::vector<std::string> icon_srcs = {
                    active_save_path + "/sce_sys/icon0.png",
                    active_save_path + "/icon0.png",
                    "/user/appmeta/" + game.title_id + "/icon0.png",
                    "/user/appmeta/" + game.title_id + "/sce_sys/icon0.png",
                    "/user/app/" + game.title_id + "/sce_sys/icon0.png",
                    "/system_data/priv/appmeta/" + game.title_id + "/icon0.png"
                };
                for (const auto &ptid : scanner::get_paired_sku_title_ids(game.title_id))
                {
                    icon_srcs.push_back("/user/appmeta/" + ptid + "/icon0.png");
                    icon_srcs.push_back("/user/appmeta/" + ptid + "/sce_sys/icon0.png");
                    icon_srcs.push_back("/user/app/" + ptid + "/sce_sys/icon0.png");
                    icon_srcs.push_back("/system_data/priv/appmeta/" + ptid + "/icon0.png");
                }
                for (const auto &isrc : icon_srcs)
                {
                    if (access(isrc.c_str(), R_OK) == 0)
                    {
                        ensure_directories_recursive(dest_game_folder + "/.meta");
                        ensure_directories_recursive(dest_game_folder + "/meta");
                        (void)pfs::copy_file_atomic(isrc, dest_game_folder + "/.meta/icon0.png", 0, 0);
                        (void)pfs::copy_file_atomic(isrc, dest_game_folder + "/meta/icon0.png", 0, 0);
                        (void)pfs::copy_file_atomic(isrc, dest_game_folder + "/icon0.png", 0, 0);
                        break;
                    }
                }
            }
        }
        else
        {
            std::string meta_path = active_save_path;
            std::size_t pos = meta_path.find("/savedata/");
            if (pos != std::string::npos)
            {
                meta_path.replace(pos, 10, "/savedata_meta/user/");
                if (access(meta_path.c_str(), F_OK) == 0)
                {
                    int meta_files = 0;
                    std::uint64_t meta_bytes = 0;
                    std::string dest_meta = dest_game_folder + "/.meta";
                    std::string dest_meta_vis = dest_game_folder + "/meta";
                    ensure_directories_recursive(dest_meta);
                    ensure_directories_recursive(dest_meta_vis);
                    copy_directory_recursive(meta_path, dest_meta, meta_files, meta_bytes, 0, nullptr);
                    int vis_files = 0;
                    std::uint64_t vis_bytes = 0;
                    copy_directory_recursive(meta_path, dest_meta_vis, vis_files, vis_bytes, 0, nullptr);
                    files_count += meta_files;
                    bytes_copied += meta_bytes;
                }
            }
        }

        // Archivar boveda criptografica SAMU (incluyendo decrypted_pfs_key si /dev/pfsmgr esta disponible)
        std::uint32_t active_user_id = extract_user_id_from_path(active_save_path);
        (void)machete::vault::archive_crypto_vault(dest_game_folder, game.title_id, game.platform,
                                                   active_save_path, active_user_id);

        // Extraer archivos desencriptados (decrypted/<slot>/) mediante montaje PFS para soporte Cross-Console universal
        const int dec_extracted = machete::pfs::extract_decrypted_slots_to_backup(
            active_save_path, dest_game_folder, game.title_id, is_ps5);
        if (dec_extracted > 0)
        {
            files_count += dec_extracted;
        }

        // Asegurar copia del keystone autentico en el respaldo
        if (access((dest_game_folder + "/keystone").c_str(), F_OK) != 0)
        {
            (void)machete::keystone::inject_authentic_keystone(dest_game_folder, game.title_id, active_save_path);
        }

        // Registrar descriptor .save_info para resolucion exacta en restauracion Cross-Console o multi-SKU
        SaveInfo sinfo;
        sinfo.game_title_id = game.title_id;
        sinfo.save_title_id = !game.save_title_id.empty() ? game.save_title_id : "";
        if (sinfo.save_title_id.empty())
        {
            const std::size_t slash = active_save_path.find_last_of('/');
            if (slash != std::string::npos && slash + 1 < active_save_path.size())
            {
                const std::string tail = active_save_path.substr(slash + 1);
                if (tail.size() == 9 && (tail.rfind("PPSA", 0) == 0 || tail.rfind("CUSA", 0) == 0))
                    sinfo.save_title_id = tail;
            }
        }
        sinfo.platform = game.platform;
        sinfo.title = game.title;
        write_save_info(dest_game_folder, sinfo);

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
            const std::string dec_tag = (dec_extracted > 0)
                ? (is_es ? " [Incluye datos desencriptados Cross-Console]" : " [Includes Cross-Console decrypted data]")
                : "";
            res.message = is_es
                              ? ("Respaldo completado (" + std::to_string(files_count) +
                                 " archivos) en " + dest_game_folder + dec_tag)
                              : ("Backup completed (" + std::to_string(files_count) +
                                 " files) at " + dest_game_folder + dec_tag);
            MACHETE_LOG("[BACKUP]", "Backup successful: %d files, %llu bytes (decrypted_files=%d)",
                        files_count, static_cast<unsigned long long>(bytes_copied), dec_extracted);
        }
        else
        {
            res.message = is_es
                              ? ("Aviso: Copia fallida o 0 archivos copiados. Revisa permisos de unidad.")
                              : ("Warning: Copy failed or 0 files copied. Check storage permissions.");
            MACHETE_LOG("[BACKUP]", "Failed to backup %s -> %s", game.title_id.c_str(), dest_game_folder.c_str());
        }

        return res;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    }
    catch (const std::exception &e)
    {
        MACHETE_LOG("[BACKUP]", "Exception during backup of %s: %s", game.title_id.c_str(), e.what());
        OperationResult res;
        res.success = false;
        res.message = std::string("Error inesperado en copia: ") + e.what();
        return res;
    }
    catch (...)
    {
        MACHETE_LOG("[BACKUP]", "Fatal unknown exception during backup of %s", game.title_id.c_str());
        OperationResult res;
        res.success = false;
        res.message = "Error crítico desconocido durante el respaldo";
        return res;
    }
#endif
}

__attribute__((noinline)) OperationResult restore_from_folder(const scanner::ScannedGame &game,
                                                               const std::string &backup_folder,
                                                               const std::string &source_label,
                                                               ProgressCallback progress_cb)
{
    MACHETE_BREADCRUMB("restore_from_folder: entry");
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    try
    {
#endif
        OperationResult res;
        const bool is_es = (machete::i18n::current_language() == machete::i18n::Language::Spanish);

        MACHETE_BREADCRUMB("restore_from_folder: verify_root");
        if (!jailbreak::verify_root_access(static_cast<int>(getpid())))
        {
            MACHETE_LOG("[RESTORE]", "Root privileges absent (UID=%d, EUID=%d). Requesting elevation...",
                        static_cast<int>(getuid()), static_cast<int>(geteuid()));
            const auto jb = jailbreak::request_jailbreak();
            if (!jb.root_access)
            {
                res.success = false;
                res.message = is_es
                    ? "Fallo de privilegios: se requiere jailbreak (UID 0) para restaurar en /user/home"
                    : "Privilege failure: jailbreak (UID 0) required to restore to /user/home";
                MACHETE_LOG("[RESTORE]", "Elevation failed. Safe operation aborted.");
                return res;
            }
        }

        if (access(backup_folder.c_str(), F_OK) != 0)
        {
            res.success = false;
            res.message = is_es ? ("No se encontro la carpeta de respaldo: " + backup_folder)
                                : ("Backup folder not found: " + backup_folder);
            MACHETE_LOG("[RESTORE]", "Backup folder inaccessible: %s", backup_folder.c_str());
            return res;
        }

        MACHETE_BREADCRUMB("restore_from_folder: calculate_size");
        const std::uint64_t total_dir_bytes = calculate_directory_size(backup_folder);
        if (total_dir_bytes == 0)
        {
            res.success = false;
            res.message = is_es ? "La carpeta de respaldo esta vacia (0 bytes)"
                                : "Backup folder is empty (0 bytes)";
            MACHETE_LOG("[RESTORE]", "Empty backup folder (0 bytes): %s", backup_folder.c_str());
            return res;
        }

        MACHETE_BREADCRUMB("restore_from_folder: resolve_target");
        const SaveInfo sinfo = read_save_info(backup_folder);
        scanner::ScannedGame eff_game = game;
        eff_game.save_title_id = game.title_id;

        std::string dst_target = eff_game.save_path;
        if (dst_target.empty())
        {
            dst_target = resolve_default_save_path(eff_game);
            MACHETE_LOG("[RESTORE]", "Target path resolved dynamically for %s (save_tid=%s): %s",
                        eff_game.title_id.c_str(), eff_game.save_title_id.c_str(), dst_target.c_str());
        }

        // Si dst_target reside en /user/home pero apunta a una carpeta de guardado de otro Title ID (ej. desvio cross-SKU),
        // asegurar que apunte estrictamente al Title ID de este juego
        if (dst_target.find("/user/home/") != std::string::npos)
        {
            std::size_t slash = dst_target.find_last_of('/');
            if (slash != std::string::npos)
            {
                std::string tail = dst_target.substr(slash + 1);
                if (tail != game.title_id && !scanner::is_paired_sku(tail, game.title_id) && (tail.rfind("PPSA", 0) == 0 || tail.rfind("CUSA", 0) == 0))
                {
                    dst_target = dst_target.substr(0, slash + 1) + game.title_id;
                    MACHETE_LOG("[RESTORE]", "Sanitized target path to match installed game title_id: %s", dst_target.c_str());
                }
            }
        }

        if (dst_target.empty())
        {
            res.success = false;
            res.message = is_es ? "Ruta de guardado del juego no localizada en la consola"
                                : "Game save path not located on console";
            MACHETE_LOG("[RESTORE]", "Could not locate save destination for %s", game.title_id.c_str());
            return res;
        }

        struct stat save_st{};
        if (stat(dst_target.c_str(), &save_st) == 0 && S_ISREG(save_st.st_mode))
        {
            std::size_t slash = dst_target.find_last_of('/');
            if (slash != std::string::npos)
                dst_target = dst_target.substr(0, slash);
        }

        MACHETE_BREADCRUMB("restore_from_folder: dummy_checks");
        const bool is_ps5 = (game.platform == "PS5" || game.title_id.rfind("PPSA", 0) == 0);
        const bool had_local_dummy = pfs::has_local_save_dummy(dst_target, is_ps5);
        const bool cross_console = pfs::is_backup_from_different_console(backup_folder, is_ps5);

        // Si es una partida de PS5 protegida por SAMU de otra consola o previa al formateo,
        // y el respaldo NO incluye carpeta decrypted/, el SAMU de la consola nunca podra descifrarla.
        // Detener inmediatamente para proteger los datos de la consola y no generar 'datos dañados'.
        if (is_ps5 && cross_console && !pfs::has_decrypted_payload(backup_folder))
        {
            res.success = false;
            res.message = is_es
                ? "Partida protegida por SAMU: Este respaldo fue creado en otra instalacion o antes de formatear la consola. El procesador criptografico SAMU de tu PS5 no puede descifrar los contenedores sdimg_* de este respaldo. Para evitar datos dañados, no se sobreescribio la partida."
                : "Save protected by SAMU: This backup was created under a different console installation or prior to formatting. The PS5 SAMU processor cannot decrypt these sdimg_* containers. Restore aborted to prevent corrupted data.";
            MACHETE_LOG("[RESTORE]", "PS5 backup sealed key not decryptable by current SAMU and has no decrypted/ payload for %s",
                        game.title_id.c_str());
            return res;
        }

        // Si es una partida de PS5 de otra consola (o solo trae decrypted/) y NO existe un Save Dummy local en la consola,
        // detener antes de escribir un sdimg_* foraneo corrupto e instruir al usuario como crear el Save Dummy
        if (is_ps5 && cross_console && !had_local_dummy)
        {
            res.success = false;
            res.message = is_es
                ? "Falta Save Dummy local: Abre el juego 1 vez en esta PS5 para que cree una partida nueva (Save Dummy), cierralo y vuelve a pulsar Restaurar."
                : "Local Save Dummy missing: Launch the game once on this PS5 to create a new save (Save Dummy), close it, and press Restore again.";
            MACHETE_LOG("[RESTORE]", "Cross-console PS5 save detected for %s without local Save Dummy in %s",
                        game.title_id.c_str(), dst_target.c_str());
            return res;
        }

        MACHETE_BREADCRUMB("restore_from_folder: resolve_ownership");
        uid_t target_uid = 0;
        gid_t target_gid = 0;
        resolve_path_ownership(dst_target, target_uid, target_gid);
        if (target_uid == 0 && target_gid == 0)
        {
            target_uid = 1;
            target_gid = 1;
        }

        MACHETE_BREADCRUMB("restore_from_folder: ensure_directories");
        ensure_directories_recursive(dst_target, target_uid, target_gid);

        // Preservar el Save Dummy local antes de copiar archivos
        const std::string preserved_dummy_dir =
            std::string(pfs::SAVE_STAGING_DIR) + "/preserved_dummy_" + game.title_id;
        if (had_local_dummy)
        {
            pfs::remove_dir_recursive_simple(preserved_dummy_dir);
            (void)pfs::preserve_local_save_dummy(dst_target, preserved_dummy_dir, is_ps5);
        }
        else
        {
            for (const std::string &ks_cand : {dst_target + "/keystone", dst_target + "/sce_sys/keystone"})
            {
                if (access(ks_cand.c_str(), R_OK) == 0 && machete::keystone::is_valid_keystone_file(ks_cand))
                {
                    pfs::ensure_dir_tree(preserved_dummy_dir);
                    (void)pfs::copy_file_atomic(ks_cand, preserved_dummy_dir + "/keystone");
                    (void)pfs::copy_file_atomic(ks_cand, preserved_dummy_dir + "/.meta_dummy/keystone");
                    pfs::ensure_dir_tree("/data/MacheteSVG/keystones");
                    (void)pfs::copy_file_atomic(ks_cand, "/data/MacheteSVG/keystones/" + game.title_id + ".keystone");
                    MACHETE_LOG("[KEYSTONE]", "Preserved authentic keystone from dst_target before restore: %s", ks_cand.c_str());
                    break;
                }
            }
        }

        int files_count = 0;
        std::uint64_t bytes_copied = 0;

        MACHETE_BREADCRUMB("restore_from_folder: log_start");
        const std::string log_src = backup_folder;
        const std::string log_dst = dst_target;
        MACHETE_LOG("[RESTORE]", "Starting restore from %s -> %s (uid=%d, gid=%d, is_ps5=%d, local_dummy=%d, cross_console=%d)",
                    log_src.c_str(), log_dst.c_str(),
                    static_cast<int>(target_uid), static_cast<int>(target_gid),
                    is_ps5 ? 1 : 0, had_local_dummy ? 1 : 0, cross_console ? 1 : 0);

        MACHETE_BREADCRUMB("restore_from_folder: copy_directory_recursive");
        const bool ok = copy_directory_recursive(backup_folder, dst_target, files_count,
                                                 bytes_copied, total_dir_bytes, progress_cb,
                                                 target_uid, target_gid);

        MACHETE_BREADCRUMB("restore_from_folder: post_process");
        pfs::PfsRestoreReport pfs_report{};
        post_process_restored_save(eff_game, dst_target, backup_folder, preserved_dummy_dir,
                                   had_local_dummy, target_uid, target_gid, &pfs_report);
        if (had_local_dummy)
        {
            pfs::remove_dir_recursive_simple(preserved_dummy_dir);
        }

        MACHETE_BREADCRUMB("restore_from_folder: sync");
        ::sync();

        files_count += pfs_report.injected_files;

        if (progress_cb)
        {
            OperationProgress p;
            p.bytes_transferred = bytes_copied > 0 ? bytes_copied : total_dir_bytes;
            p.total_bytes = total_dir_bytes > 0 ? total_dir_bytes : bytes_copied;
            p.files_transferred = files_count;
            p.progress = 1.0f;
            p.current_file = is_es ? "Completado" : "Completed";
            progress_cb(p);
        }

        res.success = ok && (files_count > 0 || pfs_report.used_save_dummy || pfs_report.used_pfs_mount);
        res.files_transferred = files_count;
        res.bytes_transferred = bytes_copied > 0 ? bytes_copied : total_dir_bytes;

        const bool has_final_keystone = (access((dst_target + "/keystone").c_str(), R_OK) == 0 &&
                                         machete::keystone::is_valid_keystone_file(dst_target + "/keystone")) ||
                                        (access((dst_target + "/sce_sys/keystone").c_str(), R_OK) == 0 &&
                                         machete::keystone::is_valid_keystone_file(dst_target + "/sce_sys/keystone"));

        if (!is_ps5 && !has_final_keystone)
        {
            if (machete::keystone::inject_fallback_fpkg_keystone(dst_target, game.title_id))
            {
                pfs_report.keystone_injected = true;
                pfs_report.fallback_keystone_used = true;
                MACHETE_LOG("[KEYSTONE]", "Injected emergency FPKG fallback keystone for %s into %s",
                            game.title_id.c_str(), dst_target.c_str());
            }
        }

        if (res.success)
        {
            if (pfs_report.used_save_dummy)
            {
                res.message = is_es
                    ? ("Restauracion Cross-Console completada en Save Dummy (" + std::to_string(files_count) +
                       " archivos) desde " + source_label)
                    : ("Cross-Console restore completed via Save Dummy (" + std::to_string(files_count) +
                       " files) from " + source_label);
            }
            else if (!is_ps5 && (pfs_report.sfo_resigned || pfs_report.keystone_injected))
            {
                if (pfs_report.fallback_keystone_used)
                {
                    res.message = is_es
                        ? ("Partida PS4 restaurada [Keystone FPKG: si tu juego es disco/PSN oficial y dice datos danados, abre el juego 1 vez y vuelve a restaurar] (" +
                           std::to_string(files_count) + " archivos)")
                        : ("PS4 save restored [FPKG Keystone: if retail disc/PSN and shows corrupted data, launch game once and restore again] (" +
                           std::to_string(files_count) + " files)");
                }
                else
                {
                    res.message = is_es
                        ? ("Partida PS4 restaurada y resignada [Modo Apollo: Account ID + Keystone Autentico] (" +
                           std::to_string(files_count) + " archivos)")
                        : ("PS4 save restored & resigned [Apollo Mode: Account ID + Authentic Keystone] (" +
                           std::to_string(files_count) + " files)");
                }
            }
            else
            {
                res.message = is_es
                    ? ("Restauracion exitosa (" + std::to_string(files_count) + " archivos) desde " + source_label)
                    : ("Restore successful (" + std::to_string(files_count) + " files) from " + source_label);
            }
            MACHETE_LOG("[RESTORE]", "Restore successful: %d files transferred (used_dummy=%d, pfs_mount=%d)",
                        files_count, pfs_report.used_save_dummy ? 1 : 0, pfs_report.used_pfs_mount ? 1 : 0);
        }
        else
        {
            res.message = is_es ? ("Fallo al restaurar archivos en " + dst_target)
                                : ("Failed to restore files at " + dst_target);
            MACHETE_LOG("[RESTORE]", "Failed to restore files at %s (files copied: %d)",
                        dst_target.c_str(), files_count);
        }

        return res;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    }
    catch (const std::exception &e)
    {
        MACHETE_LOG("[RESTORE]", "Exception during restore of %s: %s", game.title_id.c_str(), e.what());
        OperationResult res;
        res.success = false;
        res.message = std::string("Error inesperado en restauracion: ") + e.what();
        return res;
    }
    catch (...)
    {
        MACHETE_LOG("[RESTORE]", "Fatal unknown exception during restore of %s", game.title_id.c_str());
        OperationResult res;
        res.success = false;
        res.message = "Error crítico desconocido durante la restauracion";
        return res;
    }
#endif
}

OperationResult restore_game_save(const scanner::ScannedGame &game,
                                  const storage::StorageDevice &source_device,
                                  ProgressCallback progress_cb)
{
    const bool is_es = (machete::i18n::current_language() == machete::i18n::Language::Spanish);
    std::string backup_game_folder =
        source_device.root_dir + "/" + game.title_id + " - " + sanitize_name(game.title);

    if (access(backup_game_folder.c_str(), F_OK) != 0)
    {
        std::string fallback_folder = source_device.root_dir + "/" + game.title_id;
        if (access(fallback_folder.c_str(), F_OK) == 0)
            backup_game_folder = fallback_folder;
    }

    if (access(backup_game_folder.c_str(), F_OK) != 0)
    {
        OperationResult res;
        res.success = false;
        res.message = is_es ? ("No se encontro respaldo previo en " + source_device.label)
                            : ("No previous backup found on " + source_device.label);
        return res;
    }

    return restore_from_folder(game, backup_game_folder, source_device.label, progress_cb);
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

    std::string linked_save_tid;
    if (!game.save_title_id.empty() && game.save_title_id != game.title_id)
    {
        linked_save_tid = game.save_title_id;
    }
    else if (!game.save_path.empty())
    {
        const std::size_t slash = game.save_path.find_last_of('/');
        if (slash != std::string::npos && slash + 1 < game.save_path.size())
        {
            const std::string tail = game.save_path.substr(slash + 1);
            if (tail.size() == 9 && (tail.rfind("PPSA", 0) == 0 || tail.rfind("CUSA", 0) == 0) && tail != game.title_id)
                linked_save_tid = tail;
        }
    }

    for (const auto &dev : targets)
    {
        ScopedDir dir(dev.root_dir);
        if (!dir.is_valid())
            continue;

        while (const dirent *entry = readdir(dir.get()))
        {
            if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0)
                continue;

            std::string entry_name = entry->d_name;
            bool matches = (entry_name.rfind(game.title_id, 0) == 0);
            if (!matches && !linked_save_tid.empty() && entry_name.rfind(linked_save_tid, 0) == 0)
                matches = true;

            std::string full_path = dev.root_dir + "/" + entry_name;
            if (!matches)
            {
                const SaveInfo sinfo = read_save_info(full_path);
                if (sinfo.game_title_id == game.title_id ||
                    (!game.save_title_id.empty() && sinfo.save_title_id == game.save_title_id) ||
                    (!linked_save_tid.empty() && (sinfo.save_title_id == linked_save_tid || sinfo.game_title_id == linked_save_tid)))
                {
                    matches = true;
                }
            }
            if (!matches)
                continue;
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
            bool tm_ok = false;
            {
                static std::mutex s_backup_time_mutex;
                std::lock_guard<std::mutex> lock(s_backup_time_mutex);
                std::tm *tm_info = std::localtime(&st.st_mtime);
                if (tm_info)
                {
                    tm_buf = *tm_info;
                    tm_ok = true;
                }
            }
            if (tm_ok)
            {
                char time_buf[64];
                std::strftime(time_buf, sizeof(time_buf), "%d/%m/%Y, %I:%M %p", &tm_buf);
                item.timestamp_str = time_buf;
            }
            else
            {
                item.timestamp_str = is_es ? "Fecha desconocida" : "Unknown date";
            }

            int files = 0;
            std::uint64_t total_size = 0;
            bool keystone_found = false;

            ScopedDir sub(full_path);
            if (sub.is_valid())
            {
                while (const dirent *se = readdir(sub.get()))
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
                            if (std::strcmp(se->d_name, "keystone") == 0)
                                keystone_found = true;
                        }
                        else if (S_ISDIR(sst.st_mode))
                        {
                            ScopedDir sub2(sp);
                            if (sub2.is_valid())
                            {
                                while (const dirent *se2 = readdir(sub2.get()))
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
                            }
                        }
                    }
                }
            }

            item.file_count = files;
            item.total_bytes = total_size;
            item.has_keystone = keystone_found;
            item.has_decrypted = pfs::has_decrypted_payload(full_path);
            item.has_crypto_vault = (access((full_path + "/crypto_vault.bin").c_str(), R_OK) == 0);

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
    }

    // Si es un titulo de PS4 y tiene partida guardada en la consola, anadir la opcion de resignar en caliente estilo Apollo
    if ((game.platform == "PS4" || game.title_id.rfind("CUSA", 0) == 0) &&
        !game.save_path.empty() && access(game.save_path.c_str(), F_OK) == 0)
    {
        BackupItem local_opt;
        local_opt.device_id = "RESIGN_LOCAL_PS4";
        local_opt.device_label = is_es ? "Partida Activa en Consola" : "Active Save on Console";
        local_opt.storage_type = storage::StorageType::Internal;
        local_opt.folder_path = game.save_path;
        local_opt.folder_name = is_es ? "Resignar Partida Local (Modo Apollo)" : "Resign Local Save (Apollo Mode)";
        local_opt.title_id = game.title_id;
        local_opt.title = game.title;
        local_opt.timestamp_str = is_es ? "Auto-Resign / Fix" : "Auto-Resign / Fix";
        local_opt.size_str = is_es ? "Modo Apollo" : "Apollo Mode";
        local_opt.file_count = 1;
        local_opt.has_keystone = true;
        backups.insert(backups.begin(), local_opt);
    }

    return backups;
}

OperationResult resign_local_ps4_save(const scanner::ScannedGame &game,
                                      ProgressCallback progress_cb)
{
    OperationResult res;
    const bool is_es = (machete::i18n::current_language() == machete::i18n::Language::Spanish);

    if (game.platform != "PS4" && game.title_id.rfind("CUSA", 0) != 0)
    {
        res.success = false;
        res.message = is_es ? "El resignado Apollo solo aplica a juegos de PS4 (CUSAxxxxx)."
                            : "Apollo resign only applies to PS4 games (CUSAxxxxx).";
        return res;
    }

    // Resolver ruta de guardado activa
    std::string save_dir = game.save_path;
    if (save_dir.empty() || access(save_dir.c_str(), F_OK) != 0)
    {
        res.success = false;
        res.message = is_es ? "No se encontro partida activa en la consola para resignar."
                            : "No active save found on console to resign.";
        return res;
    }

    MACHETE_LOG("[RESIGN]", "Starting native Apollo Save Resign on %s (%s)...",
                game.title_id.c_str(), save_dir.c_str());

    const std::uint32_t user_id = extract_user_id_from_path(save_dir);
    const std::uint64_t active_account_id = machete::account::detect_active_account_id(user_id);

    uid_t target_uid = 0;
    gid_t target_gid = 0;
    resolve_path_ownership(save_dir, target_uid, target_gid);
    if (target_uid == 0 && target_gid == 0)
    {
        target_uid = 1;
        target_gid = 1;
    }

    if (progress_cb)
    {
        OperationProgress p;
        p.progress = 0.2f;
        p.current_file = is_es ? "Alineando Keystone autentico..." : "Aligning authentic Keystone...";
        progress_cb(p);
    }

    // 1. Alinear Keystone autentico
    (void)machete::keystone::ensure_ps4_keystone_aligned(save_dir, game.title_id, "", game.app_path);
    if (access((save_dir + "/keystone").c_str(), R_OK) == 0)
    {
        (void)chmod((save_dir + "/keystone").c_str(), 0666);
        (void)pfs::ensure_dir_tree(save_dir + "/sce_sys", target_uid, target_gid);
        (void)pfs::copy_file_atomic(save_dir + "/keystone", save_dir + "/sce_sys/keystone", target_uid, target_gid);
        (void)chmod((save_dir + "/sce_sys/keystone").c_str(), 0666);
    }

    if (progress_cb)
    {
        OperationProgress p;
        p.progress = 0.4f;
        p.current_file = is_es ? "Resignando param.sfo y recalculando HMAC..." : "Resigning param.sfo & recalculating HMAC...";
        progress_cb(p);
    }

    // 2. Resignar SFOs
    std::uint8_t console_psid[16] = {0};
    const bool has_psid = machete::vault::get_console_open_psid(console_psid);

    machete::sfo::ResignParams rp;
    rp.target_user_id = user_id;
    rp.target_account_id = active_account_id;
    rp.apply_account_id = true;
    if (has_psid)
        rp.psid = console_psid;

    bool sfo_modified = false;
    resign_sfo_directory_recursive(save_dir, rp, sfo_modified);

    if (progress_cb)
    {
        OperationProgress p;
        p.progress = 0.6f;
        p.current_file = is_es ? "Sincronizando metadatos de usuario..." : "Synchronizing user metadata...";
        progress_cb(p);
    }

    // 3. Sincronizar savedata_meta/user/<TitleID>
    char uid_hex[32];
    std::snprintf(uid_hex, sizeof(uid_hex), "%x", user_id);
    const std::string meta_ps4_user = std::string("/user/home/") + uid_hex + "/savedata_meta/user/" + game.title_id;
    ensure_directories_recursive(meta_ps4_user, target_uid, target_gid);

    // Buscar SFO valido para meta
    std::string valid_sfo;
    for (const auto &cand : {save_dir + "/sce_sys/param.sfo", save_dir + "/param.sfo"})
    {
        if (access(cand.c_str(), R_OK) == 0 && machete::sfo::is_valid_save_param_sfo(cand))
        {
            valid_sfo = cand;
            break;
        }
    }

    if (!valid_sfo.empty())
    {
        (void)pfs::copy_file_atomic(valid_sfo, meta_ps4_user + "/param.sfo", target_uid, target_gid);
        (void)machete::sfo::resign_param_sfo(meta_ps4_user + "/param.sfo", rp);
        (void)chmod((meta_ps4_user + "/param.sfo").c_str(), 0666);

        ScopedDir dslots(save_dir);
        if (dslots.is_valid())
        {
            while (const dirent *de = readdir(dslots.get()))
            {
                const std::string sname = de->d_name;
                if (machete::pfs::is_valid_ps4_slot_name(save_dir, sname))
                {
                    const std::string slot_sfo = meta_ps4_user + "/" + sname + "_param.sfo";
                    (void)pfs::copy_file_atomic(valid_sfo, slot_sfo, target_uid, target_gid);
                    (void)machete::sfo::resign_param_sfo(slot_sfo, rp);
                    (void)chmod(slot_sfo.c_str(), 0666);
                }
            }
        }
    }

    // Iconos para meta
    for (const auto &ic : {save_dir + "/sce_sys/icon0.png", save_dir + "/icon0.png"})
    {
        if (access(ic.c_str(), R_OK) == 0)
        {
            (void)pfs::copy_file_atomic(ic, meta_ps4_user + "/icon0.png", target_uid, target_gid);
            (void)pfs::copy_file_atomic(ic, meta_ps4_user + "/system0020_icon0.png", target_uid, target_gid);
            break;
        }
    }

    // Limpiar residuos en meta
    (void)unlink((meta_ps4_user + "/keystone").c_str());
    (void)unlink((meta_ps4_user + "/sce_sys/param.sfo").c_str());
    (void)unlink((meta_ps4_user + "/sce_sys/icon0.png").c_str());
    (void)rmdir((meta_ps4_user + "/sce_sys").c_str());
    apply_permissions_recursive(meta_ps4_user, target_uid, target_gid);

    if (progress_cb)
    {
        OperationProgress p;
        p.progress = 0.8f;
        p.current_file = is_es ? "Registrando en savedata.db..." : "Indexing in savedata.db...";
        progress_cb(p);
    }

    // 4. Indexar en savedata.db
    sfo::SfoSaveMetadata meta_read{};
    if (!valid_sfo.empty())
        meta_read = machete::sfo::read_sfo_metadata(valid_sfo);

    int slots_indexed = 0;
    ScopedDir dslots(save_dir);
    if (dslots.is_valid())
    {
        while (const dirent *de = readdir(dslots.get()))
        {
            const std::string sname = de->d_name;
            if (machete::pfs::is_valid_ps4_slot_name(save_dir, sname))
            {
                struct stat st_s{};
                std::uint64_t s_size = 0;
                if (stat((save_dir + "/" + sname).c_str(), &st_s) == 0)
                    s_size = static_cast<std::uint64_t>(st_s.st_size);
                db::update_or_register_ps4_savedata_db(
                    user_id, active_account_id, game.title_id, sname, s_size,
                    meta_read.valid ? &meta_read : nullptr, game.title);
                ++slots_indexed;
            }
        }
    }

    db::repair_title_corruption_flags(user_id, game.title_id, active_account_id);
    db::sanitize_database_architecture_isolation(user_id);
    apply_permissions_recursive(save_dir, target_uid, target_gid);

    ::sync();

    if (progress_cb)
    {
        OperationProgress p;
        p.progress = 1.0f;
        p.current_file = is_es ? "Completado" : "Completed";
        progress_cb(p);
    }

    char acc_hex[32];
    std::snprintf(acc_hex, sizeof(acc_hex), "%016llx", static_cast<unsigned long long>(active_account_id));

    res.success = true;
    res.files_transferred = slots_indexed;
    res.message = is_es
        ? ("Partida PS4 resignada con exito [Modo Apollo]: Account ID 0x" + std::string(acc_hex) +
           ", User ID 0x" + std::string(uid_hex) + ", Keystone sincronizado, Candado de copia desactivado y registrada en savedata.db.")
        : ("PS4 save successfully resigned [Apollo Mode]: Account ID 0x" + std::string(acc_hex) +
           ", User ID 0x" + std::string(uid_hex) + ", Keystone synced, Copy-lock unlocked and indexed in savedata.db.");

    MACHETE_LOG("[RESIGN]", "Native Apollo Resign completed successfully for %s (%s)",
                game.title_id.c_str(), res.message.c_str());
    return res;
}

__attribute__((noinline)) OperationResult restore_backup_item(const scanner::ScannedGame &game,
                                                               const BackupItem &backup,
                                                               ProgressCallback progress_cb)
{
    MACHETE_BREADCRUMB("restore_backup_item: start");
    if (backup.device_id == "RESIGN_LOCAL_PS4")
    {
        return resign_local_ps4_save(game, progress_cb);
    }
    const std::string folder = backup.folder_path;
    const std::string label = backup.device_label;
    return restore_from_folder(game, folder, label, progress_cb);
}

} // namespace machete::save
