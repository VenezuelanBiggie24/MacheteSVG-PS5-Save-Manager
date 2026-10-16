// MacheteSVG - Storage Management & USB Hotplug Implementation
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "storage.hpp"
#include "core/i18n.hpp"
#include "core/logger.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

namespace machete::storage
{

namespace
{

class ScopedDir
{
public:
    ScopedDir() noexcept : dir_(nullptr) {}
    explicit ScopedDir(DIR *dir) noexcept : dir_(dir) {}
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

void query_filesystem_space(const std::string &path, std::uint64_t &free_bytes, std::uint64_t &total_bytes)
{
    struct statvfs sv{};
    if (statvfs(path.c_str(), &sv) == 0)
    {
        free_bytes = static_cast<std::uint64_t>(sv.f_bavail) * sv.f_frsize;
        total_bytes = static_cast<std::uint64_t>(sv.f_blocks) * sv.f_frsize;
    }
}

std::string sanitize_folder_name(std::string name)
{
    const std::string invalid_chars = "\\/:*?\"<>|";
    for (char &c : name)
    {
        if (invalid_chars.find(c) != std::string::npos)
            c = '_';
    }
    return name;
}

std::uint64_t previous_usb_mask = 0;

} // namespace

std::string StorageDevice::free_space_formatted() const
{
    if (!connected)
        return "No conectado";

    if (total_bytes == 0)
        return "Listo para respaldar";

    const double gb = static_cast<double>(free_bytes) / (1024.0 * 1024.0 * 1024.0);
    char buf[64];
    if (gb >= 1.0)
        std::snprintf(buf, sizeof(buf), "%.1f GB libres", gb);
    else
    {
        const double mb = static_cast<double>(free_bytes) / (1024.0 * 1024.0);
        std::snprintf(buf, sizeof(buf), "%.0f MB libres", mb);
    }
    return std::string(buf);
}

std::string get_base_path(StorageType type)
{
    switch (type)
    {
    case StorageType::Internal:
        return std::string("/data/") + ROOT_DIR_NAME;
    case StorageType::SSD:
        return std::string("/mnt/ext1/") + ROOT_DIR_NAME;
    case StorageType::USB:
        return std::string("/mnt/usb0/") + ROOT_DIR_NAME;
    default:
        return "";
    }
}

bool ensure_internal_storage()
{
    const std::string root_dir = "/data/" + std::string(ROOT_DIR_NAME);
    if (mkdir("/data", 0777) != 0 && errno != EEXIST)
    {
        MACHETE_LOG("[STORAGE]", "mkdir /data failed: errno=%d (%s)", errno, std::strerror(errno));
    }
    chmod("/data", 0777);

    int res = mkdir(root_dir.c_str(), 0777);
    if (res != 0 && errno != EEXIST)
    {
        MACHETE_LOG("[STORAGE]", "mkdir %s failed: errno=%d (%s)", root_dir.c_str(), errno, std::strerror(errno));
    }
    chmod(root_dir.c_str(), 0777);
    return (res == 0 || errno == EEXIST || access(root_dir.c_str(), F_OK) == 0);
}

bool is_storage_available(StorageType type)
{
    if (type == StorageType::Internal)
    {
        const std::string root_dir = "/data/" + std::string(ROOT_DIR_NAME);
        return (access("/data", F_OK) == 0 || access(root_dir.c_str(), F_OK) == 0);
    }

    std::string parent_dir;
    if (type == StorageType::SSD)
        parent_dir = "/mnt/ext1";
    else if (type == StorageType::USB)
        parent_dir = "/mnt/usb0";

    return (access(parent_dir.c_str(), F_OK) == 0);
}

std::vector<StorageDevice> scan_storage_devices()
{
    std::vector<StorageDevice> devices;

    // 1. Memoria Interna (/data)
    {
        StorageDevice dev;
        dev.type = StorageType::Internal;
        dev.id = "internal";
        dev.label = machete::i18n::tr(machete::i18n::StringId::StorageInternal);
        dev.mount_path = "/data";
        dev.root_dir = "/data/" + std::string(ROOT_DIR_NAME);

        // Si /data o /data/MacheteSVG son accesibles, asegurar existencia y permisos 0777
        if (access("/data", F_OK) == 0 || access(dev.root_dir.c_str(), F_OK) == 0)
        {
            if (mkdir(dev.root_dir.c_str(), 0777) != 0 && errno != EEXIST)
            {
                MACHETE_LOG("[STORAGE]", "mkdir internal root failed: errno=%d", errno);
            }
            chmod(dev.root_dir.c_str(), 0777);
            dev.connected = true;
        }
        else
        {
            // Intentar crear /data/MacheteSVG directamente si se han elevado privilegios root
            if (mkdir(dev.root_dir.c_str(), 0777) == 0 || errno == EEXIST)
            {
                chmod(dev.root_dir.c_str(), 0777);
                dev.connected = true;
            }
            else
            {
                dev.connected = (access("/data", F_OK) == 0);
            }
        }

        if (dev.connected)
        {
            mkdir(dev.root_dir.c_str(), 0777);
            chmod(dev.root_dir.c_str(), 0777);
            query_filesystem_space("/data", dev.free_bytes, dev.total_bytes);
        }
        devices.push_back(dev);
    }

    // 2. SSD de Expansion M.2 (/mnt/ext1 o /mnt/ext0)
    {
        std::string ssd_path;
        {
            ScopedDir dir_ext1(opendir("/mnt/ext1"));
            if (dir_ext1.is_valid())
            {
                ssd_path = "/mnt/ext1";
            }
            else
            {
                ScopedDir dir_ext0(opendir("/mnt/ext0"));
                if (dir_ext0.is_valid())
                {
                    ssd_path = "/mnt/ext0";
                }
            }
        }

        if (!ssd_path.empty())
        {
            StorageDevice dev;
            dev.type = StorageType::SSD;
            dev.id = "ssd_ext1";
            dev.label = machete::i18n::tr(machete::i18n::StringId::StorageM2);
            dev.mount_path = ssd_path;
            dev.root_dir = ssd_path + "/" + ROOT_DIR_NAME;
            dev.connected = true;
            dev.free_bytes = 0;
            dev.total_bytes = 0;
            query_filesystem_space(ssd_path, dev.free_bytes, dev.total_bytes);
            devices.push_back(dev);
        }
    }

    // 3. Escaneo dinamico de puertos USB (/mnt/usb0 .. /mnt/usb7)
    for (int i = 0; i < 8; ++i)
    {
        char usb_path[32];
        std::snprintf(usb_path, sizeof(usb_path), "/mnt/usb%d", i);

        ScopedDir usb_dir(opendir(usb_path));
        if (usb_dir.is_valid())
        {
            StorageDevice dev;
            dev.type = StorageType::USB;
            dev.id = "usb" + std::to_string(i);

            char label_buf[64];
            std::snprintf(label_buf, sizeof(label_buf),
                          machete::i18n::tr(machete::i18n::StringId::StorageUsbFormat), i, usb_path);
            dev.label = label_buf;
            dev.mount_path = usb_path;
            dev.root_dir = std::string(usb_path) + "/" + ROOT_DIR_NAME;
            dev.connected = true;
            dev.free_bytes = 0;
            dev.total_bytes = 0;
            query_filesystem_space(usb_path, dev.free_bytes, dev.total_bytes);
            devices.push_back(dev);
        }
    }

    return devices;
}

bool poll_storage_changes(std::vector<StorageDevice> &out_devices, std::string &change_description)
{
    std::uint64_t current_usb_mask = 0;
    for (int i = 0; i < 8; ++i)
    {
        char usb_path[32];
        std::snprintf(usb_path, sizeof(usb_path), "/mnt/usb%d", i);
        ScopedDir dir(opendir(usb_path));
        if (dir.is_valid())
        {
            current_usb_mask |= (1ULL << i);
        }
    }

    const std::string internal_path = "/data/" + std::string(ROOT_DIR_NAME);
    const bool current_internal_connected = (access("/data", F_OK) == 0 || access(internal_path.c_str(), F_OK) == 0);

    // Primera ejecucion: solo registrar el estado inicial
    static bool first_run = true;
    static bool previous_internal_connected = false;
    if (first_run)
    {
        previous_usb_mask = current_usb_mask;
        previous_internal_connected = current_internal_connected;
        first_run = false;
        return false;
    }

    if (current_internal_connected != previous_internal_connected)
    {
        previous_internal_connected = current_internal_connected;
        out_devices = scan_storage_devices();
        if (current_internal_connected)
        {
            change_description = machete::i18n::tr(machete::i18n::StringId::StorageInternal);
            return true;
        }
    }

    if (current_usb_mask != previous_usb_mask)
    {
        out_devices = scan_storage_devices();
        for (int i = 0; i < 8; ++i)
        {
            const bool was_on = (previous_usb_mask & (1ULL << i)) != 0;
            const bool is_on = (current_usb_mask & (1ULL << i)) != 0;
            if (!was_on && is_on)
            {
                char desc_buf[128];
                std::snprintf(desc_buf, sizeof(desc_buf),
                              machete::i18n::tr(machete::i18n::StringId::UsbConnectedFormat), i);
                change_description = desc_buf;
                previous_usb_mask = current_usb_mask;
                MACHETE_LOG("[STORAGE]", "Storage device connected: USB port %d (/mnt/usb%d)", i, i);
                return true;
            }
            else if (was_on && !is_on)
            {
                char desc_buf[128];
                std::snprintf(desc_buf, sizeof(desc_buf),
                              machete::i18n::tr(machete::i18n::StringId::UsbDisconnectedFormat), i);
                change_description = desc_buf;
                previous_usb_mask = current_usb_mask;
                MACHETE_LOG("[STORAGE]", "Storage device disconnected: USB port %d (/mnt/usb%d)", i, i);
                return true;
            }
        }
        previous_usb_mask = current_usb_mask;
        return true;
    }

    return false;
}

bool ensure_directory_structure_at(const std::string &mount_path, const std::string &title_id,
                                   const std::string &game_name)
{
    if (mount_path.empty())
    {
        return false;
    }

    std::string base = mount_path + "/" + ROOT_DIR_NAME;
    if (mkdir(base.c_str(), 0777) != 0 && errno != EEXIST)
    {
        MACHETE_LOG("[STORAGE]", "mkdir base failed for %s: errno=%d (%s)",
                    base.c_str(), errno, std::strerror(errno));
    }
    chmod(base.c_str(), 0777);

    if (title_id.empty() && game_name.empty())
    {
        return true;
    }

    std::string safe_name = sanitize_folder_name(game_name);
    std::string folder_name = title_id.empty() ? safe_name : (title_id + " - " + safe_name);
    std::string game_dir = base + "/" + folder_name;
    int res = mkdir(game_dir.c_str(), 0777);
    if (res != 0 && errno != EEXIST)
    {
        MACHETE_LOG("[STORAGE]", "mkdir game_dir failed for %s: errno=%d (%s)",
                    game_dir.c_str(), errno, std::strerror(errno));
    }
    chmod(game_dir.c_str(), 0777);

    return (res == 0 || errno == EEXIST || access(game_dir.c_str(), F_OK) == 0);
}

bool ensure_directory_structure(StorageType type, const std::string &title_id,
                                const std::string &game_name)
{
    std::string mount_point;
    switch (type)
    {
    case StorageType::Internal:
        mount_point = "/data";
        break;
    case StorageType::SSD:
        mount_point = "/mnt/ext1";
        break;
    case StorageType::USB:
        mount_point = "/mnt/usb0";
        break;
    default:
        mount_point = "/data";
        break;
    }

    return ensure_directory_structure_at(mount_point, title_id, game_name);
}

} // namespace machete::storage
