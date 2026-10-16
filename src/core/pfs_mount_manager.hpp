// MacheteSVG - PFS Save Mounting, Decrypted Extraction & Save Dummy Injection Engine
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/account_manager.hpp"
#include "core/keystone_manager.hpp"
#include "core/logger.hpp"
#include "core/samu_vault.hpp"
#include "core/sfo_resigner.hpp"
#include "core/sqlite_adapter.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <mutex>



namespace machete::pfs
{

constexpr const char *SAVE_MNT_PATH = "/data/save_mnt";
constexpr const char *SAVE_STAGING_DIR = "/data/save_staging";

struct SaveMountOpt
{
    std::uint8_t reserved[8];
    const char *budgetid;
    std::uint8_t pad[48];
};

struct PfsRestoreReport
{
    bool used_pfs_mount = false;
    bool used_save_dummy = false;
    bool cross_console_detected = false;
    bool missing_save_dummy_error = false;
    bool keystone_injected = false;
    bool sfo_resigned = false;
    int injected_files = 0;
    std::string status_detail;
};

inline bool is_valid_sdimg_name(const std::string &name)
{
    if (name.rfind("sdimg_", 0) != 0)
        return false;
    if (name.rfind("sdimg_sce_bu_", 0) == 0)
        return false;
    if (name.size() > 4 && name.substr(name.size() - 4) == ".bin")
        return false;
    if (name.size() > 4 && name.substr(name.size() - 4) == ".tmp")
        return false;
    return true;
}

inline bool is_valid_ps4_slot_name(const std::string &save_dir, const std::string &name)
{
    if (name.empty() || name[0] == '.')
        return false;
    if (name.size() > 4 && (name.substr(name.size() - 4) == ".bin" || name.substr(name.size() - 4) == ".tmp"))
        return false;
    if (name.rfind("sce_bu_", 0) == 0)
        return false;
    if (name == "sealedkey" || name == "keystone" || name == "param.sfo" || name == "PARAM.SFO" || name == "icon0.png")
        return false;

    // En PS4 el contenedor de guardado debe tener un companion <name>.bin o sealedkey
    const std::string bin_path = save_dir + "/" + name + ".bin";
    const std::string sealed_path = save_dir + "/sealedkey";
    return (access(bin_path.c_str(), R_OK) == 0 || access(sealed_path.c_str(), R_OK) == 0);
}

inline bool is_valid_slot_name(const std::string &save_dir, const std::string &name, bool is_ps5)
{
    if (is_ps5)
        return is_valid_sdimg_name(name);
    return is_valid_ps4_slot_name(save_dir, name);
}

inline bool has_local_save_dummy(const std::string &save_dir, bool is_ps5, std::vector<std::string> *out_slot_names = nullptr)
{
    if (save_dir.empty())
        return false;

    DIR *dir = opendir(save_dir.c_str());
    if (!dir)
        return false;

    bool found = false;
    while (const dirent *ent = readdir(dir))
    {
        const std::string name = ent->d_name;
        if (!is_valid_slot_name(save_dir, name, is_ps5))
            continue;

        const std::string full = save_dir + "/" + name;
        struct stat st{};
        if (stat(full.c_str(), &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0)
        {
            found = true;
            if (out_slot_names)
                out_slot_names->push_back(name);
        }
    }
    closedir(dir);
    return found;
}

inline bool has_local_save_dummy(const std::string &save_dir, std::vector<std::string> *out_sdimg_names = nullptr)
{
    if (has_local_save_dummy(save_dir, true, out_sdimg_names))
        return true;
    return has_local_save_dummy(save_dir, false, out_sdimg_names);
}

inline bool directory_has_files(const std::string &dir_path)
{
    DIR *dir = opendir(dir_path.c_str());
    if (!dir)
        return false;

    bool has = false;
    while (const dirent *ent = readdir(dir))
    {
        if (std::strcmp(ent->d_name, ".") == 0 || std::strcmp(ent->d_name, "..") == 0)
            continue;
        has = true;
        break;
    }
    closedir(dir);
    return has;
}

inline bool has_decrypted_payload(const std::string &backup_folder)
{
    if (backup_folder.empty())
        return false;

    const std::string dec_dir = backup_folder + "/decrypted";
    struct stat st{};
    if (stat(dec_dir.c_str(), &st) == 0 && S_ISDIR(st.st_mode) && directory_has_files(dec_dir))
        return true;

    return false;
}

inline bool copy_file_atomic(const std::string &src, const std::string &dst, uid_t uid = 0, gid_t gid = 0)
{
    (void)uid;
    (void)gid;
    std::FILE *in = std::fopen(src.c_str(), "rb");
    if (!in)
        return false;

    const std::string tmp = dst + ".pfs_tmp";
    (void)unlink(tmp.c_str());

    std::FILE *out = std::fopen(tmp.c_str(), "wb");
    if (!out)
    {
        std::fclose(in);
        return false;
    }

    constexpr std::size_t kBufSize = 64 * 1024;
    auto buf = std::make_unique<char[]>(kBufSize);
    std::size_t n = 0;
    bool ok = true;
    while ((n = std::fread(buf.get(), 1, kBufSize, in)) > 0)
    {
        if (std::fwrite(buf.get(), 1, n, out) != n)
        {
            ok = false;
            break;
        }
    }
    if (std::ferror(in))
        ok = false;

    std::fclose(in);
    std::fflush(out);
    int fd = fileno(out);
    if (fd >= 0)
        (void)::fsync(fd);
    std::fclose(out);

    if (!ok)
    {
        (void)unlink(tmp.c_str());
        return false;
    }

    if (std::rename(tmp.c_str(), dst.c_str()) != 0)
    {
        (void)unlink(dst.c_str());
        if (std::rename(tmp.c_str(), dst.c_str()) != 0)
        {
            (void)unlink(tmp.c_str());
            return false;
        }
    }

    (void)chmod(dst.c_str(), 0666);
    return true;
}

inline void ensure_dir_tree(const std::string &path, uid_t uid = 0, gid_t gid = 0)
{
    (void)uid;
    (void)gid;
    if (path.empty())
        return;
    std::string cur;
    for (std::size_t i = 0; i < path.size(); ++i)
    {
        cur += path[i];
        if (path[i] == '/' || i == path.size() - 1)
        {
            if (cur.empty() || cur == "/")
                continue;
            struct stat st{};
            if (stat(cur.c_str(), &st) != 0)
            {
                if (mkdir(cur.c_str(), 0777) == 0)
                {
                    (void)chmod(cur.c_str(), 0777);
                }
            }
        }
    }
}

inline int copy_tree_simple(const std::string &src_dir, const std::string &dst_dir,
                            bool skip_sce_sys = false, int depth = 0)
{
    if (depth >= 8)
        return 0;

    DIR *dir = opendir(src_dir.c_str());
    if (!dir)
        return 0;

    ensure_dir_tree(dst_dir);
    int copied = 0;

    while (const dirent *ent = readdir(dir))
    {
        if (std::strcmp(ent->d_name, ".") == 0 || std::strcmp(ent->d_name, "..") == 0)
            continue;
        if (skip_sce_sys && std::strcmp(ent->d_name, "sce_sys") == 0)
            continue;

        const std::string sp = src_dir + "/" + ent->d_name;
        const std::string dp = dst_dir + "/" + ent->d_name;
        struct stat st{};
        if (stat(sp.c_str(), &st) != 0)
            continue;

        if (S_ISDIR(st.st_mode))
        {
            copied += copy_tree_simple(sp, dp, false, depth + 1);
        }
        else if (S_ISREG(st.st_mode))
        {
            if (copy_file_atomic(sp, dp))
                copied++;
        }
    }
    closedir(dir);
    return copied;
}

inline bool decrypt_pfs_key_for_slot(const std::string &save_dir,
                                     const std::string &sdimg_name,
                                     bool is_ps5,
                                     std::uint8_t out_key[32],
                                     const std::string &vault_fallback_dir = "")
{
    const std::string sdimg_path = save_dir + "/" + sdimg_name;
    std::uint8_t sealed_blob[96] = {0};
    bool blob_read = false;

    if (is_ps5)
    {
        blob_read = vault::read_sealed_blob(sdimg_path, 0x800, sealed_blob);
    }
    else
    {
        const std::string slot_name = (sdimg_name.rfind("sdimg_", 0) == 0) ? sdimg_name.substr(6) : sdimg_name;
        for (const std::string &cand : {
                 save_dir + "/" + slot_name + ".bin",
                 save_dir + "/" + sdimg_name + ".bin",
                 save_dir + "/sealedkey"})
        {
            if (vault::read_sealed_blob(cand, 0, sealed_blob))
            {
                blob_read = true;
                break;
            }
        }
        if (!blob_read)
        {
            blob_read = vault::read_sealed_blob(sdimg_path, 0x800, sealed_blob) ||
                        vault::read_sealed_blob(sdimg_path, 0x400, sealed_blob);
        }
    }

    if (blob_read && vault::decrypt_blob_via_pfsmgr(sealed_blob, out_key))
    {
        return true;
    }

    // Fallback: si el respaldo incluye crypto_vault.bin con decrypted_pfs_key (flags & 1)
    for (const std::string &vdir : {vault_fallback_dir, save_dir})
    {
        if (vdir.empty())
            continue;
        vault::CryptoVaultRecord rec{};
        if (vault::read_vault_record(vdir, rec) && (rec.flags & 1) != 0)
        {
            std::memcpy(out_key, rec.decrypted_pfs_key, 32);
            MACHETE_LOG("[PFS]", "Recovered 32-byte decrypted PFS key from crypto_vault.bin in %s", vdir.c_str());
            return true;
        }
    }

    return false;
}

inline std::mutex &pfs_global_mount_mutex()
{
    static std::mutex s_pfs_mtx;
    return s_pfs_mtx;
}

class ScopedPfsMount
{
public:
    ScopedPfsMount() = default;
    ~ScopedPfsMount()
    {
        if (mounted_)
            unmount();
    }

    ScopedPfsMount(const ScopedPfsMount &) = delete;
    ScopedPfsMount &operator=(const ScopedPfsMount &) = delete;

    bool mount(const std::string &image_path, const std::uint8_t key[32], bool write_back)
    {
        if (mounted_)
            return false;

        if (getuid() != 0)
            return false;

        static const char *candidate_paths[] = {
            "/system/vsh/libSceFsInternalForVsh.sprx",
            "/system/common/lib/libSceFsInternalForVsh.sprx",
            "/preinst2/vsh/libSceFsInternalForVsh.sprx",
            "/preinst2/common/lib/libSceFsInternalForVsh.sprx",
            "libSceFsInternalForVsh.sprx",
            nullptr
        };

        int (*init_mount)(SaveMountOpt *) = nullptr;

        for (int i = 0; candidate_paths[i]; ++i)
        {
            void *h = dlopen(candidate_paths[i], RTLD_NOW | RTLD_GLOBAL);
            if (h)
            {
                init_mount = reinterpret_cast<int (*)(SaveMountOpt *)>(dlsym(h, "sceFsInitMountSaveDataOpt"));
                mount_fn_ = reinterpret_cast<int (*)(SaveMountOpt *, const char *, const char *, const void *)>(dlsym(h, "sceFsMountSaveData"));
                init_umount_fn_ = reinterpret_cast<int (*)(SaveMountOpt *)>(dlsym(h, "sceFsInitUmountSaveDataOpt"));
                umount_fn_ = reinterpret_cast<int (*)(SaveMountOpt *, const char *, void *, int)>(dlsym(h, "sceFsUmountSaveData"));
                if (init_mount && mount_fn_ && umount_fn_)
                {
                    MACHETE_LOG("[PFS]", "Resolved libSceFsInternalForVsh symbols via dlopen (%s)", candidate_paths[i]);
                    break;
                }
            }
        }

        if (!mount_fn_ || !umount_fn_)
        {
            auto p_load = reinterpret_cast<int (*)(const char *, std::size_t, const void *, std::uint32_t, void *, int *)>(
                dlsym(RTLD_DEFAULT, "sceKernelLoadStartModule"));
            auto p_dlsym = reinterpret_cast<int (*)(int, const char *, void **)>(
                dlsym(RTLD_DEFAULT, "sceKernelDlsym"));
            if (p_load && p_dlsym)
            {
                for (int i = 0; candidate_paths[i]; ++i)
                {
                    int mod_res = 0;
                    int mod_h = p_load(candidate_paths[i], 0, nullptr, 0, nullptr, &mod_res);
                    if (mod_h > 0)
                    {
                        p_dlsym(mod_h, "sceFsInitMountSaveDataOpt", reinterpret_cast<void **>(&init_mount));
                        p_dlsym(mod_h, "sceFsMountSaveData", reinterpret_cast<void **>(&mount_fn_));
                        p_dlsym(mod_h, "sceFsInitUmountSaveDataOpt", reinterpret_cast<void **>(&init_umount_fn_));
                        p_dlsym(mod_h, "sceFsUmountSaveData", reinterpret_cast<void **>(&umount_fn_));
                        if (init_mount && mount_fn_ && umount_fn_)
                        {
                            MACHETE_LOG("[PFS]", "Resolved libSceFsInternalForVsh symbols via sceKernelLoadStartModule (%s, handle=0x%x)",
                                        candidate_paths[i], mod_h);
                            break;
                        }
                    }
                }
            }
        }

        if (!init_mount || !mount_fn_ || !umount_fn_)
        {
            const char *dl_err = dlerror();
            MACHETE_LOG("[PFS]", "libSceFsInternalForVsh.sprx not available on this host (%s)",
                        dl_err ? dl_err : "no dlerror");
            return false;
        }

        ensure_dir_tree(SAVE_STAGING_DIR);
        ensure_dir_tree(SAVE_MNT_PATH);

        // Desmontar cualquier montaje residual previo en /data/save_mnt
        {
            SaveMountOpt clean_opt{};
            if (init_umount_fn_)
                init_umount_fn_(&clean_opt);
            (void)umount_fn_(&clean_opt, SAVE_MNT_PATH, nullptr, 0);
        }

        const std::string staged_img = std::string(SAVE_STAGING_DIR) + "/sdimg_staged";
        (void)unlink(staged_img.c_str());
        if (!copy_file_atomic(image_path, staged_img))
        {
            MACHETE_LOG("[PFS]", "Failed to stage PFS image %s -> %s", image_path.c_str(), staged_img.c_str());
            return false;
        }

        SaveMountOpt mopt{};
        init_mount(&mopt);
        mopt.budgetid = "system";

        const int rc = mount_fn_(&mopt, staged_img.c_str(), SAVE_MNT_PATH, key);
        if (rc < 0)
        {
            MACHETE_LOG("[PFS]", "sceFsMountSaveData failed on %s (rc=0x%08x)",
                        staged_img.c_str(), static_cast<unsigned int>(rc));
            (void)unlink(staged_img.c_str());
            return false;
        }

        mounted_ = true;
        write_back_ = write_back;
        original_image_path_ = image_path;
        staged_image_path_ = staged_img;
        MACHETE_LOG("[PFS]", "Mounted PFS container %s at %s (write_back=%d)",
                    image_path.c_str(), SAVE_MNT_PATH, write_back ? 1 : 0);
        return true;
    }

    bool unmount()
    {
        if (!mounted_)
            return true;

        ::sync();
        SaveMountOpt uopt{};
        if (init_umount_fn_)
            init_umount_fn_(&uopt);

        int rc = 0;
        if (umount_fn_)
        {
            rc = umount_fn_(&uopt, SAVE_MNT_PATH, nullptr, 0);
        }
        ::sync();

        bool ok = (rc >= 0);
        if (write_back_ && !staged_image_path_.empty() && !original_image_path_.empty())
        {
            struct stat st{};
            if (stat(original_image_path_.c_str(), &st) == 0)
            {
                (void)copy_file_atomic(staged_image_path_, original_image_path_, st.st_uid, st.st_gid);
            }
            else
            {
                (void)copy_file_atomic(staged_image_path_, original_image_path_);
            }
            MACHETE_LOG("[PFS]", "Unmounted and committed sealed PFS container back to %s",
                        original_image_path_.c_str());
        }

        if (!staged_image_path_.empty())
            (void)unlink(staged_image_path_.c_str());
        (void)rmdir(SAVE_MNT_PATH);

        mounted_ = false;
        return ok;
    }

    std::string mount_point() const { return SAVE_MNT_PATH; }
    bool is_mounted() const { return mounted_; }

private:
    bool mounted_ = false;
    bool write_back_ = false;
    std::string original_image_path_;
    std::string staged_image_path_;
    int (*mount_fn_)(SaveMountOpt *, const char *, const char *, const void *) = nullptr;
    int (*init_umount_fn_)(SaveMountOpt *) = nullptr;
    int (*umount_fn_)(SaveMountOpt *, const char *, void *, int) = nullptr;
};

// Extrae todos los archivos desencriptados de cada slot hacia <dest_backup_folder>/decrypted/<slot>/
inline int extract_decrypted_slots_to_backup(const std::string &source_save_dir,
                                             const std::string &dest_backup_folder,
                                             const std::string &title_id,
                                             bool is_ps5)
{
    std::lock_guard<std::mutex> lock(pfs_global_mount_mutex());

    std::vector<std::string> slots;
    if (!has_local_save_dummy(source_save_dir, is_ps5, &slots))
        return 0;

    int total_extracted = 0;
    for (const auto &slot_name : slots)
    {
        std::uint8_t plain_key[32] = {0};
        if (!decrypt_pfs_key_for_slot(source_save_dir, slot_name, is_ps5, plain_key, dest_backup_folder))
        {
            MACHETE_LOG("[PFS]", "Could not decrypt PFS key for slot %s during backup extraction", slot_name.c_str());
            continue;
        }

        ScopedPfsMount mnt;
        const std::string image_path = source_save_dir + "/" + slot_name;
        if (!mnt.mount(image_path, plain_key, false))
            continue;

        const std::string slot_label = (slot_name.rfind("sdimg_", 0) == 0) ? slot_name.substr(6) : slot_name;
        const std::string out_slot_dir = dest_backup_folder + "/decrypted/" + slot_label;
        ensure_dir_tree(out_slot_dir);

        const int count = copy_tree_simple(mnt.mount_point(), out_slot_dir, false);
        total_extracted += count;

        // Preservar el keystone autentico desde el interior del contenedor PFS
        const std::string inner_ks = mnt.mount_point() + "/sce_sys/keystone";
        if (access(inner_ks.c_str(), R_OK) == 0)
        {
            (void)copy_file_atomic(inner_ks, dest_backup_folder + "/keystone");
            ensure_dir_tree("/data/MacheteSVG/keystones");
            (void)copy_file_atomic(inner_ks, "/data/MacheteSVG/keystones/" + title_id + ".keystone");
        }

        (void)mnt.unmount();
        MACHETE_LOG("[PFS]", "Extracted %d decrypted files from %s -> %s",
                    count, slot_name.c_str(), out_slot_dir.c_str());
    }

    return total_extracted;
}

// Actualiza fileSize y mtime dentro de param.json para coincidir con el sdimg_* sellado
inline void update_ps5_param_json_size(const std::string &param_json_path, std::uint64_t sdimg_size)
{
    if (sdimg_size == 0 || access(param_json_path.c_str(), R_OK) != 0)
        return;

    std::FILE *f = std::fopen(param_json_path.c_str(), "rb");
    if (!f)
        return;

    std::fseek(f, 0, SEEK_END);
    const long fsize = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (fsize <= 0 || fsize > 256 * 1024)
    {
        std::fclose(f);
        return;
    }

    std::string content(static_cast<std::size_t>(fsize), '\0');
    const std::size_t r = std::fread(&content[0], 1, content.size(), f);
    std::fclose(f);
    if (r != content.size())
        return;

    bool modified = false;
    auto replace_json_number = [&](const char *key_name, std::uint64_t new_val) {
        const std::string needle = std::string("\"") + key_name + "\"";
        std::size_t pos = content.find(needle);
        if (pos == std::string::npos)
            return;
        std::size_t colon = content.find(':', pos + needle.size());
        if (colon == std::string::npos)
            return;
        std::size_t num_start = colon + 1;
        while (num_start < content.size() && (content[num_start] == ' ' || content[num_start] == '\t'))
            num_start++;
        std::size_t num_end = num_start;
        while (num_end < content.size() && content[num_end] >= '0' && content[num_end] <= '9')
            num_end++;
        if (num_end > num_start)
        {
            content.replace(num_start, num_end - num_start, std::to_string(new_val));
            modified = true;
        }
    };

    replace_json_number("fileSize", sdimg_size);
    const std::uint64_t now_ms = static_cast<std::uint64_t>(std::time(nullptr)) * 1000ULL;
    replace_json_number("mtime", now_ms);

    if (modified)
    {
        const std::string tmp = param_json_path + ".tmp";
        std::FILE *out = std::fopen(tmp.c_str(), "wb");
        if (out)
        {
            std::fwrite(content.data(), 1, content.size(), out);
            std::fflush(out);
            int fd = fileno(out);
            if (fd >= 0)
                (void)::fsync(fd);
            std::fclose(out);
            (void)std::rename(tmp.c_str(), param_json_path.c_str());
            (void)chmod(param_json_path.c_str(), 0666);
        }
    }
}

// Comprueba si el respaldo proviene de otra consola distinta a la actual
inline bool is_backup_from_different_console(const std::string &backup_folder, bool is_ps5)
{
    // 1. Si el respaldo solo tiene archivos desencriptados (decrypted/) y ningun slot nativo, requiere inyeccion en Save Dummy
    if (has_decrypted_payload(backup_folder) && !has_local_save_dummy(backup_folder, is_ps5))
        return true;

    // 2. Comparar OpenPSID en crypto_vault.bin si ambos existen y no son cero
    vault::CryptoVaultRecord rec{};
    std::uint8_t local_psid[16] = {0};
    if (vault::read_vault_record(backup_folder, rec) && vault::get_console_open_psid(local_psid))
    {
        bool rec_has_psid = false;
        for (int i = 0; i < 16; ++i)
        {
            if (rec.open_psid[i] != 0)
            {
                rec_has_psid = true;
                break;
            }
        }
        if (rec_has_psid && std::memcmp(rec.open_psid, local_psid, 16) != 0)
        {
            MACHETE_LOG("[PFS]", "Cross-console backup detected via OpenPSID mismatch in crypto_vault.bin");
            return true;
        }
    }

    return false;
}

// Respalda temporalmente el Save Dummy local antes de que copy_directory_recursive escriba en dst_target
inline bool preserve_local_save_dummy(const std::string &dst_target, const std::string &staging_backup_dir, bool is_ps5)
{
    std::vector<std::string> local_slots;
    if (!has_local_save_dummy(dst_target, is_ps5, &local_slots))
        return false;

    ensure_dir_tree(staging_backup_dir);
    for (const auto &slot : local_slots)
    {
        (void)copy_file_atomic(dst_target + "/" + slot, staging_backup_dir + "/" + slot);
        const std::string sname = (slot.rfind("sdimg_", 0) == 0) ? slot.substr(6) : slot;
        for (const std::string &bin_cand : {
                 sname + ".bin", slot + ".bin",
                 "sce_bu_" + sname, "sce_bu_" + sname + ".bin",
                 std::string("sealedkey")})
        {
            if (access((dst_target + "/" + bin_cand).c_str(), R_OK) == 0)
            {
                (void)copy_file_atomic(dst_target + "/" + bin_cand, staging_backup_dir + "/" + bin_cand);
            }
        }
    }

    if (access((dst_target + "/sce_sys").c_str(), F_OK) == 0)
    {
        (void)copy_tree_simple(dst_target + "/sce_sys", staging_backup_dir + "/sce_sys", false);
    }
    return true;
}

inline bool preserve_local_save_dummy(const std::string &dst_target, const std::string &staging_backup_dir)
{
    if (preserve_local_save_dummy(dst_target, staging_backup_dir, true))
        return true;
    return preserve_local_save_dummy(dst_target, staging_backup_dir, false);
}

inline void remove_dir_recursive_simple(const std::string &dir_path, int depth = 0)
{
    if (depth >= 8 || dir_path.empty() || dir_path == "/")
        return;

    DIR *dir = opendir(dir_path.c_str());
    if (!dir)
        return;

    while (const dirent *ent = readdir(dir))
    {
        if (std::strcmp(ent->d_name, ".") == 0 || std::strcmp(ent->d_name, "..") == 0)
            continue;
        const std::string p = dir_path + "/" + ent->d_name;
        struct stat st{};
        if (stat(p.c_str(), &st) == 0)
        {
            if (S_ISDIR(st.st_mode))
                remove_dir_recursive_simple(p, depth + 1);
            else
                (void)unlink(p.c_str());
        }
    }
    closedir(dir);
    (void)rmdir(dir_path.c_str());
}

// Localiza la carpeta de datos desencriptados para un slot dado dentro de backup_folder/decrypted
inline std::string resolve_decrypted_slot_dir(const std::string &backup_folder, const std::string &slot_label)
{
    const std::string dec_root = backup_folder + "/decrypted";
    const std::string exact = dec_root + "/" + slot_label;
    struct stat st{};
    if (stat(exact.c_str(), &st) == 0 && S_ISDIR(st.st_mode) && directory_has_files(exact))
        return exact;

    // Si solo hay un subdirectorio dentro de decrypted/, usarlo como fuente para el Save Dummy primario
    DIR *dir = opendir(dec_root.c_str());
    if (!dir)
        return "";

    std::string first_valid;
    while (const dirent *ent = readdir(dir))
    {
        if (std::strcmp(ent->d_name, ".") == 0 || std::strcmp(ent->d_name, "..") == 0)
            continue;
        const std::string cand = dec_root + "/" + ent->d_name;
        struct stat cst{};
        if (stat(cand.c_str(), &cst) == 0 && S_ISDIR(cst.st_mode) && directory_has_files(cand))
        {
            first_valid = cand;
            break;
        }
    }
    closedir(dir);
    return first_valid;
}

// Ejecuta la inyeccion en caliente sobre Save Dummy (Cross-Console PS5/PS4) o el resignado dentro del PFS (PS4 Apollo)
inline PfsRestoreReport inject_and_resign_save(const std::string &title_id,
                                               bool is_ps5,
                                               const std::string &dst_target,
                                               const std::string &backup_folder,
                                               const std::string &preserved_dummy_dir,
                                               bool had_local_dummy,
                                               std::uint32_t user_id,
                                               std::uint64_t active_account_id,
                                               uid_t owner_uid,
                                               gid_t owner_gid)
{
    PfsRestoreReport report;
    std::lock_guard<std::mutex> lock(pfs_global_mount_mutex());

    report.cross_console_detected = is_backup_from_different_console(backup_folder, is_ps5);
    const bool has_dec = has_decrypted_payload(backup_folder);

    // 1. Si es Cross-Console (o solo trae decrypted/) y habia un Save Dummy local, restaurar el contenedor
    //    local de la consola destino para NUNCA sobrescribir su clave sellada SAMU con la de otra consola
    if (had_local_dummy && (report.cross_console_detected || has_dec) && !preserved_dummy_dir.empty())
    {
        std::vector<std::string> dummy_slots;
        if (has_local_save_dummy(preserved_dummy_dir, is_ps5, &dummy_slots))
        {
            for (const auto &slot : dummy_slots)
            {
                (void)copy_file_atomic(preserved_dummy_dir + "/" + slot,
                                       dst_target + "/" + slot, owner_uid, owner_gid);
                const std::string sname = (slot.rfind("sdimg_", 0) == 0) ? slot.substr(6) : slot;
                for (const std::string &bin_cand : {
                         sname + ".bin", slot + ".bin",
                         "sce_bu_" + sname, "sce_bu_" + sname + ".bin",
                         std::string("sealedkey")})
                {
                    if (access((preserved_dummy_dir + "/" + bin_cand).c_str(), R_OK) == 0)
                    {
                        (void)copy_file_atomic(preserved_dummy_dir + "/" + bin_cand,
                                               dst_target + "/" + bin_cand, owner_uid, owner_gid);
                    }
                }
            }
            if (is_ps5 && access((preserved_dummy_dir + "/sce_sys/param.json").c_str(), R_OK) == 0)
            {
                ensure_dir_tree(dst_target + "/sce_sys", owner_uid, owner_gid);
                (void)copy_file_atomic(preserved_dummy_dir + "/sce_sys/param.json",
                                       dst_target + "/sce_sys/param.json", owner_uid, owner_gid);
            }
            report.used_save_dummy = true;
            MACHETE_LOG("[PFS]", "Preserved local console Save Dummy container(s) in %s for Cross-Console injection",
                        dst_target.c_str());
        }
    }

    // 2. Si no hay archivos decrypted/ pre-extraidos en el backup pero necesitamos inyectar en el Save Dummy,
    //    intentar extraerlos al vuelo desde el contenedor del backup (usando /dev/pfsmgr o crypto_vault.bin)
    const std::string temp_dec_dir = std::string(SAVE_STAGING_DIR) + "/on_the_fly_dec_" + title_id;
    bool extracted_on_the_fly = false;
    if (report.used_save_dummy && !has_dec)
    {
        remove_dir_recursive_simple(temp_dec_dir);
        if (extract_decrypted_slots_to_backup(backup_folder, temp_dec_dir, title_id, is_ps5) > 0)
        {
            extracted_on_the_fly = true;
        }
    }

    const std::string effective_dec_source = extracted_on_the_fly ? temp_dec_dir : backup_folder;

    // 3. Montar cada contenedor en dst_target para inyectar datos desencriptados, keystone y resignar param.sfo
    std::vector<std::string> target_slots;
    if (has_local_save_dummy(dst_target, is_ps5, &target_slots))
    {
        std::uint8_t console_psid[16] = {0};
        const bool has_psid = vault::get_console_open_psid(console_psid);

        sfo::ResignParams rp;
        rp.target_user_id = user_id;
        rp.target_account_id = active_account_id;
        rp.apply_account_id = true;
        if (has_psid)
            rp.psid = console_psid;

        for (const auto &slot_name : target_slots)
        {
            std::uint8_t plain_key[32] = {0};
            if (!decrypt_pfs_key_for_slot(dst_target, slot_name, is_ps5, plain_key, backup_folder))
            {
                if (!is_ps5)
                {
                    struct stat st_f{};
                    std::uint64_t fsz = 0;
                    if (stat((dst_target + "/" + slot_name).c_str(), &st_f) == 0)
                        fsz = static_cast<std::uint64_t>(st_f.st_size);
                    db::update_or_register_ps4_savedata_db(user_id, active_account_id, title_id, slot_name, fsz);
                }
                continue;
            }

            ScopedPfsMount mnt;
            const std::string image_path = dst_target + "/" + slot_name;
            if (!mnt.mount(image_path, plain_key, true))
            {
                if (!is_ps5)
                {
                    struct stat st_f{};
                    std::uint64_t fsz = 0;
                    if (stat(image_path.c_str(), &st_f) == 0)
                        fsz = static_cast<std::uint64_t>(st_f.st_size);
                    db::update_or_register_ps4_savedata_db(user_id, active_account_id, title_id, slot_name, fsz);
                }
                continue;
            }

            report.used_pfs_mount = true;
            const std::string slot_label = (slot_name.rfind("sdimg_", 0) == 0) ? slot_name.substr(6) : slot_name;

            // A) Inyectar archivos desencriptados del respaldo dentro del contenedor montado
            const std::string slot_dec_dir = resolve_decrypted_slot_dir(effective_dec_source, slot_label);
            if (!slot_dec_dir.empty())
            {
                const int copied = copy_tree_simple(slot_dec_dir, mnt.mount_point(), is_ps5);
                report.injected_files += copied;
                MACHETE_LOG("[PFS]", "Injected %d decrypted files from %s into mounted %s",
                            copied, slot_dec_dir.c_str(), slot_name.c_str());
            }

            // B) Inyectar keystone autentico del juego instalado DENTRO de /data/save_mnt/sce_sys/keystone
            if (keystone::inject_authentic_keystone(mnt.mount_point(), title_id, backup_folder))
            {
                report.keystone_injected = true;
            }

            // C) Si es PS4: Resignar param.sfo DENTRO del contenedor PFS montado (Modo Apollo Save Tool)
            if (!is_ps5)
            {
                for (const std::string &inner_sfo : {
                         mnt.mount_point() + "/sce_sys/param.sfo",
                         mnt.mount_point() + "/sce_sys/PARAM.SFO",
                         mnt.mount_point() + "/param.sfo"})
                {
                    if (access(inner_sfo.c_str(), F_OK) == 0 && sfo::resign_param_sfo(inner_sfo, rp))
                    {
                        report.sfo_resigned = true;
                        MACHETE_LOG("[PFS]", "Resigned inner PS4 param.sfo inside PFS mount: %s (Account ID: 0x%04llx...%04llx)",
                                    inner_sfo.c_str(),
                                    static_cast<unsigned long long>((active_account_id >> 48) & 0xffff),
                                    static_cast<unsigned long long>(active_account_id & 0xffff));
                    }
                }
            }

            // D) Desmontar para que SAMU recalcule el arbol Merkle y selle el contenedor con la clave de esta consola
            (void)mnt.unmount();

            // E) Actualizar metadatos externos y base de datos del sistema
            struct stat st_sd{};
            if (stat(image_path.c_str(), &st_sd) == 0 && st_sd.st_size > 0)
            {
                if (is_ps5)
                {
                    update_ps5_param_json_size(dst_target + "/sce_sys/param.json",
                                               static_cast<std::uint64_t>(st_sd.st_size));
                    update_ps5_param_json_size(dst_target + "/param.json",
                                               static_cast<std::uint64_t>(st_sd.st_size));
                }
                else
                {
                    // Si habia un backup slot sce_bu_<slot>, sincronizarlo con el contenedor recién sellado
                    const std::string bu_img = dst_target + "/sce_bu_" + slot_name;
                    const std::string bu_bin = dst_target + "/sce_bu_" + slot_name + ".bin";
                    const std::string main_bin = dst_target + "/" + slot_name + ".bin";
                    if (access(bu_img.c_str(), F_OK) == 0)
                    {
                        (void)copy_file_atomic(image_path, bu_img, owner_uid, owner_gid);
                        if (access(main_bin.c_str(), R_OK) == 0)
                            (void)copy_file_atomic(main_bin, bu_bin, owner_uid, owner_gid);
                    }

                    // Indexar en savedata.db de PS4 garantizando is_broken = 0
                    db::update_or_register_ps4_savedata_db(user_id, active_account_id, title_id, slot_name,
                                                           static_cast<std::uint64_t>(st_sd.st_size));
                }
            }
        }
    }

    if (!is_ps5)
    {
        // Limpieza final de banderas de corrupcion para este Title ID en todas las bases de datos
        db::repair_title_corruption_flags(user_id, title_id);
    }

    if (extracted_on_the_fly)
    {
        remove_dir_recursive_simple(temp_dec_dir);
    }

    return report;
}

} // namespace machete::pfs

