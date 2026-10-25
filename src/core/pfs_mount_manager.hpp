// MacheteSVG - PFS Save Mounting, Decrypted Extraction & Save Dummy Injection Engine
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/account_manager.hpp"
#include "core/aes_xts.hpp"
#include "core/keystone_manager.hpp"
#include "core/logger.hpp"
#include "core/samu_vault.hpp"
#include "core/sfo_resigner.hpp"
#include "core/sqlite_adapter.hpp"

#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <signal.h>
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
    bool fallback_keystone_used = false;
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
    if (name.rfind("sce_bu_", 0) == 0 || name.rfind("sdimg_sce_bu_", 0) == 0)
        return false;
    if (name == "sealedkey" || name == "keystone" || name == "param.sfo" || name == "PARAM.SFO" || name == "icon0.png")
        return false;

    const std::string clean_name = (name.rfind("sdimg_", 0) == 0) ? name.substr(6) : name;
    if (clean_name.empty() || clean_name.rfind("sce_bu_", 0) == 0)
        return false;

    // En PS4/PS5 BC el contenedor de guardado (ej. sdimg_Book o Book) tiene un companion <slot>.bin (ej. Book.bin)
    const std::string bin_path_clean = save_dir + "/" + clean_name + ".bin";
    const std::string bin_path_raw = save_dir + "/" + name + ".bin";
    const std::string sealed_path = save_dir + "/sealedkey";
    return (access(bin_path_clean.c_str(), R_OK) == 0 ||
            access(bin_path_raw.c_str(), R_OK) == 0 ||
            access(sealed_path.c_str(), R_OK) == 0);
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
    if (uid != 0 || gid != 0)
        (void)chown(dst.c_str(), uid, gid);
    return true;
}

inline void ensure_dir_tree(const std::string &path, uid_t uid = 0, gid_t gid = 0)
{
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
                    if (uid != 0 || gid != 0)
                        (void)chown(cur.c_str(), uid, gid);
                }
            }
            else
            {
                (void)chmod(cur.c_str(), 0777);
                if (uid != 0 || gid != 0)
                    (void)chown(cur.c_str(), uid, gid);
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
        off_t found_off = 0;
        blob_read = vault::read_ps5_sealed_blob(sdimg_path, sealed_blob, &found_off);
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
            blob_read = vault::read_ps5_sealed_blob(sdimg_path, sealed_blob);
        }
    }

    if (blob_read && vault::decrypt_blob_via_pfsmgr(sealed_blob, out_key, is_ps5))
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

#if defined(__PROSPERO__) || defined(__Prospero__) || defined(__PS5__)
extern "C"
{
    int sceKernelLoadStartModule(const char *name, std::size_t argc, const void *argv,
                                 std::uint32_t flags, void *opt, int *pRes);
    int sceKernelDlsym(int handle, const char *symbol, void **addrp);
    int sceKernelGetModuleList(int * handles, std::size_t num_handles, std::size_t *out_count);
    int sceSysmoduleLoadModuleInternal(std::uint32_t id);
    const char *sceKernelGetFsSandboxRandomWord(void);
}
#endif

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

    void set_write_back(bool wb) noexcept
    {
        write_back_ = wb;
    }

    static bool ensure_pfs_symbols_resolved(
        int (**out_init_mount)(SaveMountOpt *),
        int (**out_mount)(SaveMountOpt *, const char *, const char *, const void *),
        int (**out_init_umount)(SaveMountOpt *),
        int (**out_umount)(SaveMountOpt *, const char *, void *, int))
    {
        static int (*s_init_mount)(SaveMountOpt *) = nullptr;
        static int (*s_mount_fn)(SaveMountOpt *, const char *, const char *, const void *) = nullptr;
        static int (*s_init_umount)(SaveMountOpt *) = nullptr;
        static int (*s_umount_fn)(SaveMountOpt *, const char *, void *, int) = nullptr;
        static bool s_already_resolved = false;

        if (s_already_resolved && s_mount_fn && s_umount_fn)
        {
            if (out_init_mount) *out_init_mount = s_init_mount;
            if (out_mount) *out_mount = s_mount_fn;
            if (out_init_umount) *out_init_umount = s_init_umount;
            if (out_umount) *out_umount = s_umount_fn;
            return true;
        }

#if defined(__PROSPERO__) || defined(__Prospero__) || defined(__PS5__)
        // Pre-cargar el modulo interno SCE_SYSMODULE_INTERNAL_FS_INTERNAL (0x80000066)
        int sys_rc = sceSysmoduleLoadModuleInternal(0x80000066U);
        MACHETE_LOG("[PFS]", "sceSysmoduleLoadModuleInternal(0x80000066) -> 0x%08x",
                    static_cast<unsigned int>(sys_rc));
#endif

        std::vector<std::string> dynamic_paths;
#if defined(__PROSPERO__) || defined(__Prospero__) || defined(__PS5__)
        const char *rnd_word = sceKernelGetFsSandboxRandomWord();
        if (rnd_word && rnd_word[0] != '\0')
        {
            dynamic_paths.push_back(std::string("/") + rnd_word + "/priv_ex/lib/libSceFsInternalForVsh.sprx");
            dynamic_paths.push_back(std::string("/") + rnd_word + "/common_ex/lib/libSceFsInternalForVsh.sprx");
            dynamic_paths.push_back(std::string("/") + rnd_word + "/priv/lib/libSceFsInternalForVsh.sprx");
            dynamic_paths.push_back(std::string("/") + rnd_word + "/common/lib/libSceFsInternalForVsh.sprx");
            dynamic_paths.push_back(std::string("/") + rnd_word + "/priv_ex/lib/libSceFsInternal.sprx");
            dynamic_paths.push_back(std::string("/") + rnd_word + "/common_ex/lib/libSceFsInternal.sprx");
            dynamic_paths.push_back(std::string("/") + rnd_word + "/common/lib/libSceFsInternal.sprx");
            dynamic_paths.push_back(std::string("/") + rnd_word + "/common/lib/libSceSaveData.sprx");
            dynamic_paths.push_back(std::string("/") + rnd_word + "/common_ex/lib/libSceSaveData.sprx");
        }
#endif
        static const char *static_candidate_paths[] = {
            "/system_ex/priv_ex/lib/libSceFsInternalForVsh.sprx",
            "/system_ex/common_ex/lib/libSceFsInternalForVsh.sprx",
            "/system_ex/vsh_ex/lib/libSceFsInternalForVsh.sprx",
            "/system_ex/common/lib/libSceFsInternalForVsh.sprx",
            "/system_ex/priv/lib/libSceFsInternalForVsh.sprx",
            "/system_ex/app_ex/lib/libSceFsInternalForVsh.sprx",
            "/system/vsh/libSceFsInternalForVsh.sprx",
            "/system/common/lib/libSceFsInternalForVsh.sprx",
            "/system/priv/lib/libSceFsInternalForVsh.sprx",
            "/preinst2/vsh/libSceFsInternalForVsh.sprx",
            "/preinst2/common/lib/libSceFsInternalForVsh.sprx",
            "/preinst2/priv/lib/libSceFsInternalForVsh.sprx",
            "/system/vsh/module/libSceFsInternalForVsh.sprx",
            "/system_ex/common_ex/lib/libSceFsInternal.sprx",
            "/system_ex/priv_ex/lib/libSceFsInternal.sprx",
            "/system/priv/lib/libSceFsInternal.sprx",
            "/system/common/lib/libSceFsInternal.sprx",
            "/system/sys/libSceFsInternalForVsh.sprx",
            "/system/sys/libSceFsInternal.sprx",
            "/system_ex/priv_ex/lib/libSceSaveData.sprx",
            "/system_ex/common_ex/lib/libSceSaveData.sprx",
            "/system/common/lib/libSceSaveData.sprx",
            "/system/priv/lib/libSceSaveData.sprx",
            "libSceFsInternalForVsh.sprx",
            "libSceFsInternal.sprx",
            "libSceSaveData.sprx",
            "libSceSaveDataInternal.sprx",
            nullptr
        };
        for (int i = 0; static_candidate_paths[i]; ++i)
        {
            dynamic_paths.push_back(static_candidate_paths[i]);
        }

        // Incluir tanto el nombre en texto plano como los NIDs reales en little-endian y big-endian
        static const char *kInitMountSyms[] = {
            "sceFsInitMountSaveDataOpt",
            "sceSaveDataInitMountOpt",
            "+mPswO7rxKs",
            "-mPswO7rxKs",
            "q8Tr7sDsY/o",
            "q8Tr7sDsY/o=",
            nullptr
        };
        static const char *kMountSyms[] = {
            "sceFsMountSaveData",
            "sceSaveDataMount",
            "sceSaveDataMountInternal",
            "c7YjLnWxw/4",
            "c7YjLnWxw-4",
            "/sOxdS4jtnM",
            "/sOxdS4jtnM=",
            nullptr
        };
        static const char *kInitUmountSyms[] = {
            "sceFsInitUmountSaveDataOpt",
            "sceSaveDataInitUmountOpt",
            "udWWfVZ5cPg",
            "+HB5Vn2W1bk",
            "+HB5Vn2W1bk=",
            nullptr
        };
        static const char *kUmountSyms[] = {
            "sceFsUmountSaveData",
            "sceSaveDataUmount",
            "sceSaveDataUmountInternal",
            "Ck7qGp2MjI4",
            "joyMnRrqTgo",
            "joyMnRrqTgo=",
            nullptr
        };

        auto probe_handle = [&](void *h, const char *source_tag) -> bool {
            if (!h)
                return false;
            int (*found_init_m)(SaveMountOpt *) = nullptr;
            int (*found_m)(SaveMountOpt *, const char *, const char *, const void *) = nullptr;
            int (*found_init_u)(SaveMountOpt *) = nullptr;
            int (*found_u)(SaveMountOpt *, const char *, void *, int) = nullptr;

            for (int k = 0; kInitMountSyms[k]; ++k)
            {
                void *s = dlsym(h, kInitMountSyms[k]);
                if (s) { found_init_m = reinterpret_cast<int (*)(SaveMountOpt *)>(s); break; }
            }
            for (int k = 0; kMountSyms[k]; ++k)
            {
                void *s = dlsym(h, kMountSyms[k]);
                if (s) { found_m = reinterpret_cast<int (*)(SaveMountOpt *, const char *, const char *, const void *)>(s); break; }
            }
            for (int k = 0; kInitUmountSyms[k]; ++k)
            {
                void *s = dlsym(h, kInitUmountSyms[k]);
                if (s) { found_init_u = reinterpret_cast<int (*)(SaveMountOpt *)>(s); break; }
            }
            for (int k = 0; kUmountSyms[k]; ++k)
            {
                void *s = dlsym(h, kUmountSyms[k]);
                if (s) { found_u = reinterpret_cast<int (*)(SaveMountOpt *, const char *, void *, int)>(s); break; }
            }

            if (found_m && found_u)
            {
                s_init_mount = found_init_m;
                s_mount_fn = found_m;
                s_init_umount = found_init_u;
                s_umount_fn = found_u;
                s_already_resolved = true;
                if (out_init_mount) *out_init_mount = s_init_mount;
                if (out_mount) *out_mount = s_mount_fn;
                if (out_init_umount) *out_init_umount = s_init_umount;
                if (out_umount) *out_umount = s_umount_fn;
                MACHETE_LOG("[PFS]", "Resolved PFS mount symbols via %s (mount=%p, umount=%p, init_m=%p, init_u=%p)",
                            source_tag, reinterpret_cast<void *>(s_mount_fn), reinterpret_cast<void *>(s_umount_fn),
                            reinterpret_cast<void *>(s_init_mount), reinterpret_cast<void *>(s_init_umount));
                return true;
            }
            return false;
        };

        // 1. Probar en los simbolos ya cargados en el proceso
        #ifdef RTLD_DEFAULT
        if (probe_handle(RTLD_DEFAULT, "RTLD_DEFAULT"))
            return true;
        #endif

        // 2. Probar cargando con dlopen las rutas candidatas del sistema
        for (const std::string &cand_str : dynamic_paths)
        {
            const char *cand = cand_str.c_str();
            (void)dlerror();
            void *h = dlopen(cand, RTLD_NOW | RTLD_GLOBAL);
            if (!h)
                h = dlopen(cand, RTLD_LAZY | RTLD_GLOBAL);
            if (h)
            {
                MACHETE_LOG("[PFS]", "dlopen('%s') succeeded (handle=%p)", cand, h);
                if (probe_handle(h, cand))
                    return true;
            }
        }

#if defined(__PROSPERO__) || defined(__Prospero__) || defined(__PS5__)
        auto probe_kernel_mod = [&](int mod_handle, const char *tag) -> bool {
            if (mod_handle < 0)
                return false;
            int (*found_init_m)(SaveMountOpt *) = nullptr;
            int (*found_m)(SaveMountOpt *, const char *, const char *, const void *) = nullptr;
            int (*found_init_u)(SaveMountOpt *) = nullptr;
            int (*found_u)(SaveMountOpt *, const char *, void *, int) = nullptr;

            for (int k = 0; kInitMountSyms[k]; ++k)
            {
                void *s = nullptr;
                if (sceKernelDlsym(mod_handle, kInitMountSyms[k], &s) == 0 && s)
                {
                    found_init_m = reinterpret_cast<int (*)(SaveMountOpt *)>(s);
                    break;
                }
            }
            for (int k = 0; kMountSyms[k]; ++k)
            {
                void *s = nullptr;
                if (sceKernelDlsym(mod_handle, kMountSyms[k], &s) == 0 && s)
                {
                    found_m = reinterpret_cast<int (*)(SaveMountOpt *, const char *, const char *, const void *)>(s);
                    break;
                }
            }
            for (int k = 0; kInitUmountSyms[k]; ++k)
            {
                void *s = nullptr;
                if (sceKernelDlsym(mod_handle, kInitUmountSyms[k], &s) == 0 && s)
                {
                    found_init_u = reinterpret_cast<int (*)(SaveMountOpt *)>(s);
                    break;
                }
            }
            for (int k = 0; kUmountSyms[k]; ++k)
            {
                void *s = nullptr;
                if (sceKernelDlsym(mod_handle, kUmountSyms[k], &s) == 0 && s)
                {
                    found_u = reinterpret_cast<int (*)(SaveMountOpt *, const char *, void *, int)>(s);
                    break;
                }
            }

            if (found_m && found_u)
            {
                s_init_mount = found_init_m;
                s_mount_fn = found_m;
                s_init_umount = found_init_u;
                s_umount_fn = found_u;
                s_already_resolved = true;
                if (out_init_mount) *out_init_mount = s_init_mount;
                if (out_mount) *out_mount = s_mount_fn;
                if (out_init_umount) *out_init_umount = s_init_umount;
                if (out_umount) *out_umount = s_umount_fn;
                MACHETE_LOG("[PFS]", "Resolved PFS mount symbols via kernel module: %s (handle=%d)", tag, mod_handle);
                return true;
            }
            return false;
        };

        // 3. Probar mediante sceKernelLoadStartModule de libkernel
        for (const std::string &cand_str : dynamic_paths)
        {
            const char *cand = cand_str.c_str();
            int res_out = 0;
            int mod_handle = sceKernelLoadStartModule(cand, 0, nullptr, 0, nullptr, &res_out);
            if (mod_handle >= 0 || static_cast<unsigned int>(mod_handle) == 0x80020023U)
            {
                MACHETE_LOG("[PFS]", "sceKernelLoadStartModule('%s') -> 0x%08x (res_out=%d)",
                            cand, static_cast<unsigned int>(mod_handle), res_out);
                void *h = dlopen(cand, RTLD_NOW | RTLD_GLOBAL);
                if (!h)
                    h = dlopen(cand, RTLD_LAZY | RTLD_GLOBAL);
                if (!h)
                    h = dlopen("libSceFsInternalForVsh.sprx", RTLD_NOW | RTLD_GLOBAL);
                if (!h)
                    h = dlopen("libSceFsInternalForVsh.sprx", RTLD_LAZY | RTLD_GLOBAL);
                if (h && probe_handle(h, cand))
                    return true;
                if (mod_handle >= 0 && probe_kernel_mod(mod_handle, cand))
                    return true;
            }
        }

        // 4. Escanear todos los modulos ya cargados en el proceso mediante sceKernelGetModuleList
        int handles[256] = {0};
        std::size_t num_loaded = 0;
        if (sceKernelGetModuleList(handles, 256, &num_loaded) == 0 && num_loaded > 0)
        {
            for (std::size_t m = 0; m < num_loaded && m < 256; ++m)
            {
                if (probe_kernel_mod(handles[m], "sceKernelGetModuleList"))
                    return true;
            }
        }
#endif

        const char *err = dlerror();
        MACHETE_LOG("[PFS]", "libSceFsInternalForVsh.sprx symbols unavailable (%s)",
                    err ? err : "no matching module");
        return false;
    }

    bool mount(const std::string &image_path, const std::uint8_t key[32], bool write_back)
    {
        if (mounted_)
            return false;

        (void)setuid(0);
        (void)seteuid(0);

        int (*init_mount)(SaveMountOpt *) = nullptr;
        if (!ensure_pfs_symbols_resolved(&init_mount, &mount_fn_, &init_umount_fn_, &umount_fn_))
            return false;

        ensure_dir_tree(SAVE_STAGING_DIR);
        ensure_dir_tree(SAVE_MNT_PATH);

        // Desmontar cualquier montaje residual previo en /data/save_mnt
        {
            struct stat ma{}, mb{};
            if (stat(SAVE_MNT_PATH, &ma) == 0 && stat("/data", &mb) == 0 && ma.st_dev != mb.st_dev)
            {
                SaveMountOpt clean_opt{};
                if (init_umount_fn_)
                    init_umount_fn_(&clean_opt);
                (void)umount_fn_(&clean_opt, SAVE_MNT_PATH, nullptr, 0);
                ::sync();
            }
        }

        std::string base_name = image_path;
        std::string parent_dir;
        const std::size_t last_slash = image_path.find_last_of('/');
        if (last_slash != std::string::npos)
        {
            parent_dir = image_path.substr(0, last_slash);
            base_name = image_path.substr(last_slash + 1);
        }
        if (base_name.empty())
            base_name = "sdimg_staged";

        const std::string clean_slot = (base_name.rfind("sdimg_", 0) == 0) ? base_name.substr(6) : base_name;
        // En PS5 sceFsMountSaveData suele esperar prefijo sdimg_<slot>; en PS4 puede esperar <slot> puro
        const std::string staged_img = std::string(SAVE_STAGING_DIR) + "/sdimg_" + clean_slot;
        const std::string staged_raw = std::string(SAVE_STAGING_DIR) + "/" + clean_slot;
        (void)unlink(staged_img.c_str());
        (void)unlink(staged_raw.c_str());
        if (!copy_file_atomic(image_path, staged_img))
        {
            MACHETE_LOG("[PFS]", "Failed to stage PFS image %s -> %s", image_path.c_str(), staged_img.c_str());
            return false;
        }
        (void)chmod(staged_img.c_str(), 0755);
        (void)copy_file_atomic(image_path, staged_raw);
        (void)chmod(staged_raw.c_str(), 0755);

        // Si existe archivo companion <slot>.bin (PS4/PS5 BC), copiarlo junto a la imagen en staging
        original_bin_path_.clear();
        staged_bin_path_.clear();
        std::string src_bin = parent_dir.empty() ? (clean_slot + ".bin") : (parent_dir + "/" + clean_slot + ".bin");
        if (access(src_bin.c_str(), R_OK) != 0)
            src_bin = image_path + ".bin";
        if (access(src_bin.c_str(), R_OK) == 0)
        {
            const std::string dst_bin_clean = std::string(SAVE_STAGING_DIR) + "/" + clean_slot + ".bin";
            const std::string dst_bin_sdimg = staged_img + ".bin";
            (void)unlink(dst_bin_clean.c_str());
            (void)unlink(dst_bin_sdimg.c_str());
            if (copy_file_atomic(src_bin, dst_bin_clean))
            {
                (void)chmod(dst_bin_clean.c_str(), 0755);
                (void)copy_file_atomic(src_bin, dst_bin_sdimg);
                (void)chmod(dst_bin_sdimg.c_str(), 0755);
                original_bin_path_ = src_bin;
                staged_bin_path_ = dst_bin_clean;
            }
        }

        SaveMountOpt mopt{};
        if (init_mount)
            init_mount(&mopt);
        mopt.budgetid = "system";

        (void)signal(SIGPIPE, SIG_IGN);
        int rc = mount_fn_(&mopt, staged_img.c_str(), SAVE_MNT_PATH, key);
        bool dev_mounted = (rc >= 0);
        std::string staged_chosen = staged_img;
        if (!dev_mounted)
        {
            SaveMountOpt mopt_raw{};
            if (init_mount)
                init_mount(&mopt_raw);
            mopt_raw.budgetid = "system";
            const int rc2 = mount_fn_(&mopt_raw, staged_raw.c_str(), SAVE_MNT_PATH, key);
            if (rc2 >= 0)
            {
                rc = rc2;
                dev_mounted = true;
                staged_chosen = staged_raw;
            }
        }
        if (dev_mounted)
        {
            struct stat ma{}, mb{};
            if (stat(SAVE_MNT_PATH, &ma) == 0 && stat("/data", &mb) == 0 && ma.st_dev == mb.st_dev)
            {
                dev_mounted = false;
            }
        }

        if (!dev_mounted)
        {
            MACHETE_LOG("[PFS]", "sceFsMountSaveData failed on %s (rc=0x%08x)",
                        staged_img.c_str(), static_cast<unsigned int>(rc));
            (void)unlink(staged_img.c_str());
            (void)unlink(staged_raw.c_str());
            if (!staged_bin_path_.empty())
                (void)unlink(staged_bin_path_.c_str());
            return false;
        }

        mounted_ = true;
        write_back_ = write_back;
        original_image_path_ = image_path;
        staged_image_path_ = staged_chosen;
        staged_raw_path_ = staged_raw;
        MACHETE_LOG("[PFS]", "Mounted PFS container %s at %s (write_back=%d, rc=%d)",
                    image_path.c_str(), SAVE_MNT_PATH, write_back ? 1 : 0, rc);
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
            if (!staged_bin_path_.empty() && !original_bin_path_.empty() && access(staged_bin_path_.c_str(), R_OK) == 0)
            {
                (void)copy_file_atomic(staged_bin_path_, original_bin_path_);
            }
            MACHETE_LOG("[PFS]", "Unmounted and committed sealed PFS container back to %s (rc=%d)",
                        original_image_path_.c_str(), rc);
        }
        else
        {
            MACHETE_LOG("[PFS]", "Unmounted PFS container %s (read-only / untouched, rc=%d)",
                        original_image_path_.c_str(), rc);
        }

        if (!staged_image_path_.empty())
            (void)unlink(staged_image_path_.c_str());
        if (!staged_raw_path_.empty())
            (void)unlink(staged_raw_path_.c_str());
        if (!staged_bin_path_.empty())
            (void)unlink(staged_bin_path_.c_str());
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
    std::string staged_raw_path_;
    std::string original_bin_path_;
    std::string staged_bin_path_;
    int (*mount_fn_)(SaveMountOpt *, const char *, const char *, const void *) = nullptr;
    int (*init_umount_fn_)(SaveMountOpt *) = nullptr;
    int (*umount_fn_)(SaveMountOpt *, const char *, void *, int) = nullptr;
};

// Implementacion interna de extraccion desencriptada (asume que pfs_global_mount_mutex ya esta adquirido)
inline int extract_decrypted_slots_unlocked(const std::string &source_save_dir,
                                            const std::string &dest_backup_folder,
                                            const std::string &title_id,
                                            bool is_ps5)
{
    std::vector<std::string> slots;
    if (!has_local_save_dummy(source_save_dir, is_ps5, &slots))
        return 0;

    int total_extracted = 0;
    for (const auto &slot_name : slots)
    {
        std::uint8_t plain_key[32] = {0};
        if (!decrypt_pfs_key_for_slot(source_save_dir, slot_name, is_ps5, plain_key, dest_backup_folder))
        {
            MACHETE_LOG("[PFS]", "Could not decrypt PFS key for slot %s in %s",
                        slot_name.c_str(), source_save_dir.c_str());
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
        if (access(inner_ks.c_str(), R_OK) == 0 && keystone::is_valid_keystone_file(inner_ks))
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

// Extrae todos los archivos desencriptados de cada slot hacia <dest_backup_folder>/decrypted/<slot>/
inline int extract_decrypted_slots_to_backup(const std::string &source_save_dir,
                                             const std::string &dest_backup_folder,
                                             const std::string &title_id,
                                             bool is_ps5)
{
    std::lock_guard<std::mutex> lock(pfs_global_mount_mutex());
    return extract_decrypted_slots_unlocked(source_save_dir, dest_backup_folder, title_id, is_ps5);
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

// Actualiza titleId dentro de param.json para coincidir con un SKU emparejado o destino
inline void update_ps5_param_json_title_id(const std::string &param_json_path, const std::string &new_title_id)
{
    if (new_title_id.empty() || access(param_json_path.c_str(), R_OK) != 0)
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

    const std::string needle = "\"titleId\":";
    std::size_t pos = content.find(needle);
    if (pos != std::string::npos)
    {
        std::size_t q1 = content.find('"', pos + needle.size());
        if (q1 != std::string::npos)
        {
            std::size_t q2 = content.find('"', q1 + 1);
            if (q2 != std::string::npos)
            {
                content.replace(q1 + 1, q2 - (q1 + 1), new_title_id);
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
                    (void)unlink(param_json_path.c_str());
                    (void)std::rename(tmp.c_str(), param_json_path.c_str());
                    (void)chmod(param_json_path.c_str(), 0666);
                }
            }
        }
    }
}

// Actualiza dirName dentro de param.json para apuntar al slot principal de partida (ej. AUTOSAVE0)
inline void update_ps5_param_json_dir_name(const std::string &param_json_path, const std::string &new_dir_name)
{
    if (new_dir_name.empty() || access(param_json_path.c_str(), R_OK) != 0)
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

    const std::string needle = "\"dirName\":";
    std::size_t pos = content.find(needle);
    if (pos != std::string::npos)
    {
        std::size_t q1 = content.find('"', pos + needle.size());
        if (q1 != std::string::npos)
        {
            std::size_t q2 = content.find('"', q1 + 1);
            if (q2 != std::string::npos)
            {
                content.replace(q1 + 1, q2 - (q1 + 1), new_dir_name);
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
                    (void)unlink(param_json_path.c_str());
                    (void)std::rename(tmp.c_str(), param_json_path.c_str());
                    (void)chmod(param_json_path.c_str(), 0666);
                }
            }
        }
    }
}

// Comprueba si el respaldo proviene de otra consola distinta a la actual
inline bool is_backup_from_different_console(const std::string &backup_folder, bool is_ps5, const std::string &title_id = "")
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
    else if (is_ps5 && !title_id.empty())
    {
        // En PS5, si el respaldo carece de crypto_vault.bin local y de carpeta decrypted/,
        // verificar el fingerprint de 32 bytes dentro del keystone (offset 0x2C).
        // Si el fingerprint del keystone del respaldo difiere del keystone autentico de esta consola,
        // la partida fue generada bajo otra instalacion/consola y SAMU rechazara los sdimg_*.
        const std::string backup_ks = backup_folder + "/keystone";
        std::uint8_t b_ks[96] = {0};
        std::uint8_t l_ks[96] = {0};
        if (keystone::is_valid_keystone_file(backup_ks))
        {
            std::FILE *fb = std::fopen(backup_ks.c_str(), "rb");
            if (fb)
            {
                (void)std::fread(b_ks, 1, 96, fb);
                std::fclose(fb);
            }
            const std::string local_ks_path = keystone::find_authentic_game_keystone(title_id);
            if (!local_ks_path.empty() && keystone::is_valid_keystone_file(local_ks_path))
            {
                std::FILE *fl = std::fopen(local_ks_path.c_str(), "rb");
                if (fl)
                {
                    (void)std::fread(l_ks, 1, 96, fl);
                    std::fclose(fl);
                }
                bool b_has_fp = false, l_has_fp = false;
                for (int k = 0x2C; k < 0x4C; ++k)
                {
                    if (b_ks[k] != 0) b_has_fp = true;
                    if (l_ks[k] != 0) l_has_fp = true;
                }
                if (b_has_fp && l_has_fp && std::memcmp(b_ks + 0x2C, l_ks + 0x2C, 32) != 0)
                {
                    MACHETE_LOG("[PFS]", "Cross-console PS5 backup detected via Keystone fingerprint mismatch at offset 0x2C");
                    return true;
                }
            }
        }
    }

    // 3. Verificacion criptografica en hardware con SAMU (/dev/pfsmgr):
    //    Probar si el SAMU de ESTA consola puede descifrar la clave sellada (.bin en PS4).
    //    NOTA CRITICA: ioctl 0xc0845302 solo soporta claves selladas de PS4 (formato pfsSKKey v10 / 0x0A).
    //    En PS5 (formato pfsSKKey v1 / 0x01) el kernel devuelve EINVAL / status_0x80 = -53, por lo que
    //    esta prueba de hardware NUNCA debe ejecutarse para PS5 (solo para PS4 / !is_ps5).
    if (!is_ps5 && access("/dev/pfsmgr", F_OK) == 0 && getuid() == 0)
    {
        std::vector<std::string> bslots;
        if (has_local_save_dummy(backup_folder, is_ps5, &bslots) && !bslots.empty())
        {
            std::uint8_t test_key[32] = {0};
            if (!decrypt_pfs_key_for_slot(backup_folder, bslots[0], is_ps5, test_key))
            {
                MACHETE_LOG("[PFS]", "Backup PS4 sealed key rejected by local SAMU (fresh OS reinstall or foreign console): %s",
                            bslots[0].c_str());
                return true;
            }
        }
    }

    return false;
}

// Respalda temporalmente el Save Dummy local antes de que copy_directory_recursive escriba en dst_target
inline bool preserve_local_save_dummy(const std::string &dst_target, const std::string &staging_backup_dir, bool is_ps5)
{
    // Preservar keystone autentico del destino local si existe (vital para restauraciones de PS4)
    for (const std::string &ks_cand : {
             dst_target + "/keystone",
             dst_target + "/sce_sys/keystone"})
    {
        if (access(ks_cand.c_str(), R_OK) == 0 && keystone::is_valid_keystone_file(ks_cand))
        {
            ensure_dir_tree(staging_backup_dir);
            (void)copy_file_atomic(ks_cand, staging_backup_dir + "/keystone");
            (void)copy_file_atomic(ks_cand, staging_backup_dir + "/.meta_dummy/keystone");
            ensure_dir_tree("/data/MacheteSVG/keystones");
            std::string save_tid;
            const std::size_t slash = dst_target.find_last_of('/');
            if (slash != std::string::npos)
                save_tid = dst_target.substr(slash + 1);
            if (save_tid.size() == 9)
                (void)copy_file_atomic(ks_cand, "/data/MacheteSVG/keystones/" + save_tid + ".keystone");
            MACHETE_LOG("[KEYSTONE]", "Preserved authentic local keystone from %s -> %s/keystone",
                        ks_cand.c_str(), staging_backup_dir.c_str());
            break;
        }
    }

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
                 "sdimg_sce_bu_" + sname, "sdimg_sce_bu_" + sname + ".bin",
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

    // Preservar tambien los metadatos actuales del Save Dummy en savedata_prospero_meta / savedata_meta
    if (is_ps5)
    {
        std::string meta_path = dst_target;
        const std::size_t pos = meta_path.find("/savedata_prospero/");
        if (pos != std::string::npos)
        {
            std::string meta_direct = meta_path;
            meta_direct.replace(pos, 19, "/savedata_prospero_meta/");
            std::string meta_user = meta_path;
            meta_user.replace(pos, 19, "/savedata_prospero_meta/user/");
            if (access(meta_user.c_str(), F_OK) == 0)
                (void)copy_tree_simple(meta_user, staging_backup_dir + "/.meta_dummy", false);
            else if (access(meta_direct.c_str(), F_OK) == 0)
                (void)copy_tree_simple(meta_direct, staging_backup_dir + "/.meta_dummy", false);
        }
    }
    else
    {
        std::string meta_path = dst_target;
        const std::size_t pos = meta_path.find("/savedata/");
        if (pos != std::string::npos)
        {
            meta_path.replace(pos, 10, "/savedata_meta/user/");
            if (access(meta_path.c_str(), F_OK) == 0)
                (void)copy_tree_simple(meta_path, staging_backup_dir + "/.meta_dummy", false);
        }
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
    int sub_count = 0;
    while (const dirent *ent = readdir(dir))
    {
        if (std::strcmp(ent->d_name, ".") == 0 || std::strcmp(ent->d_name, "..") == 0)
            continue;
        const std::string cand = dec_root + "/" + ent->d_name;
        struct stat cst{};
        if (stat(cand.c_str(), &cst) == 0 && S_ISDIR(cst.st_mode) && directory_has_files(cand))
        {
            if (first_valid.empty())
                first_valid = cand;
            sub_count++;
        }
    }
    closedir(dir);
    return (sub_count == 1) ? first_valid : "";
}

// Registra en el log los archivos presentes en /data/save_mnt/sce_sys para diagnostico
inline void log_mounted_sce_sys_contents(const std::string &mount_root, const std::string &slot_name)
{
    const std::string sys_dir = mount_root + "/sce_sys";
    DIR *d = opendir(sys_dir.c_str());
    if (!d)
        return;

    std::string list_str;
    while (const dirent *ent = readdir(d))
    {
        if (ent->d_name[0] == '.')
            continue;
        if (!list_str.empty())
            list_str += ", ";
        list_str += ent->d_name;
    }
    closedir(d);
    if (!list_str.empty())
    {
        MACHETE_LOG("[PFS]", "Mounted %s/sce_sys contents: [%s]", slot_name.c_str(), list_str.c_str());
    }
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

    report.cross_console_detected = is_backup_from_different_console(backup_folder, is_ps5, title_id);
    const bool has_dec = has_decrypted_payload(backup_folder);
    const bool has_backup_meta =
        (access((backup_folder + "/.meta/param.json").c_str(), R_OK) == 0 ||
         access((backup_folder + "/meta/param.json").c_str(), R_OK) == 0 ||
         access((backup_folder + "/.meta/sce_sys/param.json").c_str(), R_OK) == 0);

    // 1. Si habia un Save Dummy local, cosechar su keystone autentico de 96 bytes
    //    y su PARAMS.psid_hmac / account_id nativo de esta consola e instalacion
    std::string harvested_keystone_path;
    sfo::SfoSaveMetadata dummy_sfo_meta{};
    std::vector<std::string> dummy_slots;
    if (had_local_dummy && !preserved_dummy_dir.empty())
    {
        has_local_save_dummy(preserved_dummy_dir, is_ps5, &dummy_slots);

        const std::string dummy_meta_dir = preserved_dummy_dir + "/.meta_dummy";
        for (const std::string &cand_ks : {
                 dummy_meta_dir + "/keystone",
                 dummy_meta_dir + "/sce_sys/keystone",
                 preserved_dummy_dir + "/keystone",
                 preserved_dummy_dir + "/sce_sys/keystone"})
        {
            if (keystone::is_valid_keystone_file(cand_ks))
            {
                harvested_keystone_path = preserved_dummy_dir + "/harvested_keystone";
                (void)copy_file_atomic(cand_ks, harvested_keystone_path);
                ensure_dir_tree("/data/MacheteSVG/keystones");
                (void)copy_file_atomic(cand_ks, "/data/MacheteSVG/keystones/" + title_id + ".keystone");
                MACHETE_LOG("[KEYSTONE]", "Harvested authentic 96-byte keystone from local Save Dummy (%s)",
                            cand_ks.c_str());
                break;
            }
        }

        for (const std::string &cand_sfo : {
                 dummy_meta_dir + "/param.sfo",
                 dummy_meta_dir + "/sce_sys/param.sfo",
                 preserved_dummy_dir + "/param.sfo",
                 preserved_dummy_dir + "/sce_sys/param.sfo"})
        {
            if (access(cand_sfo.c_str(), R_OK) == 0)
            {
                dummy_sfo_meta = sfo::read_sfo_metadata(cand_sfo);
                if (dummy_sfo_meta.valid)
                {
                    MACHETE_LOG("[PFS]", "Harvested local Save Dummy SFO metadata: acc=0x%04llx...%04llx, uid=0x%x, has_hmac=%d",
                                static_cast<unsigned long long>((dummy_sfo_meta.account_id >> 48) & 0xffff),
                                static_cast<unsigned long long>(dummy_sfo_meta.account_id & 0xffff),
                                dummy_sfo_meta.user_id,
                                dummy_sfo_meta.has_psid_hmac ? 1 : 0);
                    break;
                }
            }
        }

        for (const std::string &cand_json : {
                 dummy_meta_dir + "/param.json",
                 dummy_meta_dir + "/sce_sys/param.json",
                 preserved_dummy_dir + "/param.json",
                 preserved_dummy_dir + "/sce_sys/param.json"})
        {
            if (access(cand_json.c_str(), R_OK) == 0)
            {
                ensure_dir_tree(preserved_dummy_dir + "/sce_sys");
                (void)copy_file_atomic(cand_json, preserved_dummy_dir + "/sce_sys/param.json");
                break;
            }
        }
    }

    // 2. Si el respaldo no tiene carpeta decrypted/ pre-extraida, extraer al vuelo desde los contenedores del backup
    const std::string temp_dec_dir = std::string(SAVE_STAGING_DIR) + "/on_the_fly_dec_" + title_id;
    bool extracted_on_the_fly = false;
    if (!has_dec && (!is_ps5 || report.cross_console_detected || !has_backup_meta || had_local_dummy))
    {
        remove_dir_recursive_simple(temp_dec_dir);
        if (extract_decrypted_slots_unlocked(backup_folder, temp_dec_dir, title_id, is_ps5) > 0)
        {
            extracted_on_the_fly = true;
        }
    }

    const std::string effective_dec_source = extracted_on_the_fly ? temp_dec_dir : backup_folder;
    const bool have_any_decrypted = (has_dec || extracted_on_the_fly);

    // 3. Si habia un Save Dummy local y:
    //    - Es un juego de PS4 (siempre preferir el contenedor nativo del Save Dummy local + inyeccion de archivos), O
    //    - Es un juego de PS5 Cross-Console o pre-formateo sin .meta/ y tenemos archivos desencriptados
    if (had_local_dummy && !dummy_slots.empty() && have_any_decrypted &&
        (!is_ps5 || report.cross_console_detected || !has_backup_meta))
    {
        for (const auto &slot : dummy_slots)
        {
            const std::string slot_label = (slot.rfind("sdimg_", 0) == 0) ? slot.substr(6) : slot;
            // En PS5 solo pisar con el dummy aquellos slots para los que tengamos datos desencriptados correspondientes
            if (is_ps5 && resolve_decrypted_slot_dir(effective_dec_source, slot_label).empty())
                continue;

            (void)copy_file_atomic(preserved_dummy_dir + "/" + slot,
                                   dst_target + "/" + slot, owner_uid, owner_gid);
            const std::string sname = (slot.rfind("sdimg_", 0) == 0) ? slot.substr(6) : slot;
            for (const std::string &bin_cand : {
                     sname + ".bin", slot + ".bin",
                     "sce_bu_" + sname, "sce_bu_" + sname + ".bin",
                     "sdimg_sce_bu_" + sname, "sdimg_sce_bu_" + sname + ".bin",
                     std::string("sealedkey")})
            {
                if (access((preserved_dummy_dir + "/" + bin_cand).c_str(), R_OK) == 0)
                {
                    (void)copy_file_atomic(preserved_dummy_dir + "/" + bin_cand,
                                           dst_target + "/" + bin_cand, owner_uid, owner_gid);
                }
            }
            report.used_save_dummy = true;
        }
        if (report.used_save_dummy)
        {
            MACHETE_LOG("[PFS]", "Restored local console Save Dummy container(s) in %s for plaintext injection",
                        dst_target.c_str());
        }
    }

    // 4. Montar cada contenedor en dst_target para inyectar datos desencriptados, keystone y resignar param.sfo
    std::vector<std::string> target_slots;
    if (has_local_save_dummy(dst_target, is_ps5, &target_slots))
    {
        std::uint8_t console_psid[16] = {0};
        const bool has_psid = vault::get_console_open_psid(console_psid);

        if (active_account_id == 0 && dummy_sfo_meta.valid && dummy_sfo_meta.account_id != 0)
        {
            active_account_id = dummy_sfo_meta.account_id;
        }

        sfo::ResignParams rp;
        rp.target_user_id = (user_id != 0) ? user_id : dummy_sfo_meta.user_id;
        rp.target_account_id = active_account_id;
        rp.apply_account_id = true;
        if (has_psid)
            rp.psid = console_psid;
        if (dummy_sfo_meta.has_psid_hmac)
            rp.exact_psid_hmac = dummy_sfo_meta.psid_hmac;

        for (const auto &slot_name : target_slots)
        {
            std::uint8_t plain_key[32] = {0};
            if (!decrypt_pfs_key_for_slot(dst_target, slot_name, is_ps5, plain_key, backup_folder))
            {
                struct stat st_f{};
                std::uint64_t fsz = 0;
                if (stat((dst_target + "/" + slot_name).c_str(), &st_f) == 0)
                    fsz = static_cast<std::uint64_t>(st_f.st_size);
                if (is_ps5)
                {
                    db::update_or_register_ps5_savedata_db(user_id, active_account_id, title_id, slot_name, fsz);
                }
                else
                {
                    db::update_or_register_ps4_savedata_db(user_id, active_account_id, title_id, slot_name, fsz);
                }
                continue;
            }

            ScopedPfsMount mnt;
            const std::string image_path = dst_target + "/" + slot_name;
            if (!mnt.mount(image_path, plain_key, true))
            {
                struct stat st_f{};
                std::uint64_t fsz = 0;
                if (stat(image_path.c_str(), &st_f) == 0)
                    fsz = static_cast<std::uint64_t>(st_f.st_size);

                // No alterar sectores crudos de contenedores PFS sin montar, ya que un contenedor PFS
                // con flags 0x000D (Merkle HMAC) quedaria invalidado ante el kernel si se modifica fuera de sceFsMountSaveData.
                if (is_ps5)
                {
                    db::update_or_register_ps5_savedata_db(user_id, active_account_id, title_id, slot_name, fsz);
                }
                else
                {
                    db::update_or_register_ps4_savedata_db(user_id, active_account_id, title_id, slot_name, fsz,
                                                           dummy_sfo_meta.valid ? &dummy_sfo_meta : nullptr);
                }
                continue;
            }

            report.used_pfs_mount = true;
            const std::string slot_label = (slot_name.rfind("sdimg_", 0) == 0) ? slot_name.substr(6) : slot_name;
            log_mounted_sce_sys_contents(mnt.mount_point(), slot_name);

            const std::string inner_sfo_path = mnt.mount_point() + "/sce_sys/param.sfo";
            const sfo::SfoSaveMetadata inner_meta = sfo::read_sfo_metadata(inner_sfo_path);

            if (is_ps5)
            {
                // Determinar si este slot de PS5 necesita modificacion (Cross-Console, pre-formateo o cambio de cuenta)
                bool needs_ps5_modify = report.used_save_dummy || report.cross_console_detected || !has_backup_meta;
                if (inner_meta.valid)
                {
                    if (active_account_id != 0 && inner_meta.account_id != 0 && inner_meta.account_id != active_account_id)
                        needs_ps5_modify = true;
                    if (user_id != 0 && inner_meta.user_id != 0 && inner_meta.user_id != user_id)
                        needs_ps5_modify = true;
                }

                // Exportar param.json e icon0.png internos hacia savedata_prospero_meta si el backup no tenia .meta
                char uid_hex[32];
                std::snprintf(uid_hex, sizeof(uid_hex), "%x", user_id);
                const std::string meta_target_user = std::string("/user/home/") + uid_hex + "/savedata_prospero_meta/user/" + title_id;
                ensure_dir_tree(meta_target_user, owner_uid, owner_gid);

                const std::string inner_json = mnt.mount_point() + "/sce_sys/param.json";
                if (access(inner_json.c_str(), R_OK) == 0)
                {
                    (void)copy_file_atomic(inner_json, meta_target_user + "/param.json", owner_uid, owner_gid);
                }
                const std::string inner_icon = mnt.mount_point() + "/sce_sys/icon0.png";
                if (access(inner_icon.c_str(), R_OK) == 0 && access((meta_target_user + "/icon0.png").c_str(), R_OK) != 0)
                {
                    (void)copy_file_atomic(inner_icon, meta_target_user + "/icon0.png", owner_uid, owner_gid);
                }

                if (!needs_ps5_modify)
                {
                    mnt.set_write_back(false);
                    (void)mnt.unmount();
                    struct stat st_sd{};
                    if (stat(image_path.c_str(), &st_sd) == 0 && st_sd.st_size > 0)
                    {
                        db::update_or_register_ps5_savedata_db(
                            user_id, active_account_id, title_id, slot_name,
                            static_cast<std::uint64_t>(st_sd.st_size));
                    }
                    continue;
                }

                // Inyectar archivos desencriptados si el contenedor actual proviene del Save Dummy
                const std::string slot_dec_dir = resolve_decrypted_slot_dir(effective_dec_source, slot_label);
                if (!slot_dec_dir.empty())
                {
                    const int copied = copy_tree_simple(slot_dec_dir, mnt.mount_point(), true);
                    report.injected_files += copied;
                    MACHETE_LOG("[PFS]", "Injected %d decrypted files from %s into mounted PS5 slot %s",
                                 copied, slot_dec_dir.c_str(), slot_name.c_str());
                }

                // Inyectar keystone autentico cosechado del Save Dummy o del juego
                if (!harvested_keystone_path.empty() && keystone::is_valid_keystone_file(harvested_keystone_path))
                {
                    ensure_dir_tree(mnt.mount_point() + "/sce_sys");
                    if (copy_file_atomic(harvested_keystone_path, mnt.mount_point() + "/sce_sys/keystone"))
                        report.keystone_injected = true;
                }
                else if (keystone::inject_authentic_keystone(mnt.mount_point(), title_id, backup_folder))
                {
                    report.keystone_injected = true;
                }

                // Si existe param.sfo dentro del contenedor PS5, resignarlo con el Account ID y User ID activos
                if (access(inner_sfo_path.c_str(), F_OK) == 0 && sfo::resign_param_sfo(inner_sfo_path, rp))
                {
                    report.sfo_resigned = true;
                    MACHETE_LOG("[PFS]", "Resigned inner PS5 param.sfo inside PFS mount: %s", slot_name.c_str());
                }

                (void)mnt.unmount();

                struct stat st_sd{};
                if (stat(image_path.c_str(), &st_sd) == 0 && st_sd.st_size > 0)
                {
                    // Sincronizar backup slot sdimg_sce_bu_<slot> si existe
                    const std::string bu_img = dst_target + "/sdimg_sce_bu_" + slot_label;
                    if (access(bu_img.c_str(), F_OK) == 0)
                    {
                        (void)copy_file_atomic(image_path, bu_img, owner_uid, owner_gid);
                    }

                    update_ps5_param_json_size(meta_target_user + "/param.json",
                                               static_cast<std::uint64_t>(st_sd.st_size));
                    db::update_or_register_ps5_savedata_db(
                        user_id, active_account_id, title_id, slot_name,
                        static_cast<std::uint64_t>(st_sd.st_size));
                }
            }
            else
            {
                // Flujo PS4 (Modo Apollo Save Tool + Inyeccion en Save Dummy)
                // Si el contenedor montado es el Save Dummy local, cosechar su psid_hmac, account_id, user_id
                // y su keystone autentico ANTES de sobrescribir param.sfo con el del backup
                if (report.used_save_dummy)
                {
                    if (inner_meta.valid)
                    {
                        dummy_sfo_meta = inner_meta;
                        if (dummy_sfo_meta.has_psid_hmac)
                            rp.exact_psid_hmac = dummy_sfo_meta.psid_hmac;
                        if (active_account_id == 0 && dummy_sfo_meta.account_id != 0)
                        {
                            active_account_id = dummy_sfo_meta.account_id;
                            rp.target_account_id = active_account_id;
                        }
                        if (rp.target_user_id == 0 && dummy_sfo_meta.user_id != 0)
                            rp.target_user_id = dummy_sfo_meta.user_id;
                    }
                    const std::string inner_ks = mnt.mount_point() + "/sce_sys/keystone";
                    if (harvested_keystone_path.empty() && keystone::is_valid_keystone_file(inner_ks))
                    {
                        harvested_keystone_path = std::string(SAVE_STAGING_DIR) + "/harvested_ks_" + title_id;
                        (void)copy_file_atomic(inner_ks, harvested_keystone_path);
                        ensure_dir_tree("/data/MacheteSVG/keystones");
                        (void)copy_file_atomic(inner_ks, "/data/MacheteSVG/keystones/" + title_id + ".keystone");
                        MACHETE_LOG("[KEYSTONE]", "Harvested authentic keystone from mounted PS4 Save Dummy (%s)",
                                    slot_name.c_str());
                    }
                }

                const std::string slot_dec_dir = resolve_decrypted_slot_dir(effective_dec_source, slot_label);
                if (!slot_dec_dir.empty())
                {
                    const int copied = copy_tree_simple(slot_dec_dir, mnt.mount_point(), true);
                    report.injected_files += copied;

                    // Copiar tambien param.sfo e icon0.png del backup al interior del contenedor para conservar
                    // MAINTITLE, SUBTITLE, DETAIL y SAVEDATA_LIST_PARAM (user_param) de la partida respaldada
                    const std::string dec_sfo = slot_dec_dir + "/sce_sys/param.sfo";
                    if (sfo::is_valid_save_param_sfo(dec_sfo))
                    {
                        ensure_dir_tree(mnt.mount_point() + "/sce_sys");
                        (void)copy_file_atomic(dec_sfo, mnt.mount_point() + "/sce_sys/param.sfo");
                    }
                    const std::string dec_icon = slot_dec_dir + "/sce_sys/icon0.png";
                    if (access(dec_icon.c_str(), R_OK) == 0)
                    {
                        ensure_dir_tree(mnt.mount_point() + "/sce_sys");
                        (void)copy_file_atomic(dec_icon, mnt.mount_point() + "/sce_sys/icon0.png");
                    }
                    MACHETE_LOG("[PFS]", "Injected %d decrypted files from %s into mounted PS4 slot %s",
                                copied, slot_dec_dir.c_str(), slot_name.c_str());
                }

                // Inyectar keystone autentico de 96 bytes dentro de /data/save_mnt/sce_sys/keystone
                if (!harvested_keystone_path.empty() && keystone::is_valid_keystone_file(harvested_keystone_path))
                {
                    ensure_dir_tree(mnt.mount_point() + "/sce_sys");
                    if (copy_file_atomic(harvested_keystone_path, mnt.mount_point() + "/sce_sys/keystone"))
                    {
                        report.keystone_injected = true;
                        MACHETE_LOG("[KEYSTONE]", "Injected harvested Save Dummy keystone into %s/sce_sys/keystone",
                                    slot_name.c_str());
                    }
                }
                else if (keystone::inject_authentic_keystone(mnt.mount_point(), title_id, backup_folder))
                {
                    report.keystone_injected = true;
                }

                // Resignar param.sfo DENTRO del contenedor PFS montado (ACCOUNT_ID, user_id, PARAMS.psid_hmac)
                sfo::SfoSaveMetadata resigned_meta{};
                for (const std::string &inner_sfo : {
                         mnt.mount_point() + "/sce_sys/param.sfo",
                         mnt.mount_point() + "/sce_sys/PARAM.SFO",
                         mnt.mount_point() + "/param.sfo"})
                {
                    if (access(inner_sfo.c_str(), F_OK) == 0 && sfo::resign_param_sfo(inner_sfo, rp))
                    {
                        report.sfo_resigned = true;
                        resigned_meta = sfo::read_sfo_metadata(inner_sfo);
                        MACHETE_LOG("[PFS]", "Resigned inner PS4 param.sfo inside PFS mount: %s (Account ID: 0x%04llx...%04llx)",
                                    inner_sfo.c_str(),
                                    static_cast<unsigned long long>((active_account_id >> 48) & 0xffff),
                                    static_cast<unsigned long long>(active_account_id & 0xffff));
                    }
                }

                // Exportar <slot>_icon0.png hacia savedata_meta/user/<TitleID>/ antes de desmontar
                const std::string inner_icon = mnt.mount_point() + "/sce_sys/icon0.png";
                if (access(inner_icon.c_str(), R_OK) == 0)
                {
                    char uid_hex[32];
                    std::snprintf(uid_hex, sizeof(uid_hex), "%x", user_id);
                    const std::string meta_ps4 = std::string("/user/home/") + uid_hex + "/savedata_meta/user/" + title_id;
                    ensure_dir_tree(meta_ps4, owner_uid, owner_gid);
                    (void)copy_file_atomic(inner_icon, meta_ps4 + "/" + slot_label + "_icon0.png", owner_uid, owner_gid);
                }

                // Desmontar para que el kernel recalcule el arbol Merkle y selle el contenedor
                (void)mnt.unmount();

                struct stat st_sd{};
                if (stat(image_path.c_str(), &st_sd) == 0 && st_sd.st_size > 0)
                {
                    // Sincronizar el backup slot sdimg_sce_bu_<slot> (PS5 BC) o sce_bu_<slot> (PS4)
                    const std::string bu_sdimg = dst_target + "/sdimg_sce_bu_" + slot_label;
                    const std::string bu_plain = dst_target + "/sce_bu_" + slot_label;
                    const std::string bu_bin = dst_target + "/sce_bu_" + slot_label + ".bin";
                    const std::string main_bin = dst_target + "/" + slot_label + ".bin";
                    if (access(bu_sdimg.c_str(), F_OK) == 0 || slot_name.rfind("sdimg_", 0) == 0)
                    {
                        (void)copy_file_atomic(image_path, bu_sdimg, owner_uid, owner_gid);
                    }
                    else if (access(bu_plain.c_str(), F_OK) == 0)
                    {
                        (void)copy_file_atomic(image_path, bu_plain, owner_uid, owner_gid);
                    }
                    if (access(main_bin.c_str(), R_OK) == 0)
                    {
                        (void)copy_file_atomic(main_bin, bu_bin, owner_uid, owner_gid);
                    }

                    // Actualizar savedata.db preservando metadatos reales de la partida (main_title, sub_title, user_param, free_blocks=0)
                    db::update_or_register_ps4_savedata_db(
                        user_id, active_account_id, title_id, slot_label,
                        static_cast<std::uint64_t>(st_sd.st_size),
                        resigned_meta.valid ? &resigned_meta : (dummy_sfo_meta.valid ? &dummy_sfo_meta : nullptr));
                }
            }
        }

#if defined(__PROSPERO__) || defined(__Prospero__) || defined(__PS5__)
        // Si es una partida de PS5 de otra consola, NINGUN contenedor sdimg_* pudo ser montado por el kernel
        // y el respaldo tampoco incluye carpeta decrypted/ en texto plano,
        // entonces los contenedores sdimg_* estan sellados con la clave SAMU de otra consola y no pueden usarse crudos.
        if (is_ps5 && report.cross_console_detected && !report.used_pfs_mount && !have_any_decrypted)
        {
            report.missing_save_dummy_error = true;
            MACHETE_LOG("[PFS]", "Foreign encrypted PS5 containers in %s cannot be mounted on this console without decrypted/ payload",
                        backup_folder.c_str());
        }
#endif
    }

    // Limpieza final de banderas de corrupcion (is_broken, fake_broken) y sincronizacion de account_id en savedata.db
    db::repair_title_corruption_flags(user_id, title_id, active_account_id);

    if (extracted_on_the_fly)
    {
        remove_dir_recursive_simple(temp_dec_dir);
    }

    return report;
}

} // namespace machete::pfs

