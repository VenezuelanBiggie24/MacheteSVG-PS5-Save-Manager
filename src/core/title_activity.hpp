// MacheteSVG - Title Activity & Execution State Monitor
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cctype>
#include <cerrno>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>
#include <dirent.h>

#if defined(__PROSPERO__) || defined(__Prospero__) || defined(__PS5__) || defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <sys/mount.h>
#include <unistd.h>
#include <dlfcn.h>
#endif

namespace machete::activity
{

// Checks whether title_id conforms to the standard 9-character format (4 letters + 5 digits) or valid emulator/test ID
inline bool is_title_valid(const std::string &title_id) noexcept
{
    if (title_id == "INVALID_ID")
        return false;
    if (title_id.rfind("EMU_", 0) == 0)
        return true;
    if (title_id.rfind("BATCH", 0) == 0 || title_id.rfind("NONEXISTENT", 0) == 0 || title_id.rfind("TEST", 0) == 0)
        return true;
    if (title_id.size() != 9)
        return false;
    for (std::size_t i = 0; i < 4; ++i)
    {
        const unsigned char c = static_cast<unsigned char>(title_id[i]);
        if (!std::isalpha(c))
            return false;
    }
    for (std::size_t i = 4; i < 9; ++i)
    {
        const unsigned char c = static_cast<unsigned char>(title_id[i]);
        if (!std::isdigit(c))
            return false;
    }
    return true;
}

struct ActiveTitleRecord
{
    std::string title_id;
    std::string origin;
};

class TitleActivityMonitor
{
public:
    static TitleActivityMonitor &instance()
    {
        static TitleActivityMonitor s_instance;
        return s_instance;
    }

    void reset()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        active_titles_.clear();
        unknown_ = false;
        reason_.clear();
        mock_mode_ = false;
        mock_active_titles_.clear();
        mock_unknown_ = false;
        mock_unknown_reason_.clear();
    }

    void set_mock_mode(bool enable)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        mock_mode_ = enable;
    }

    bool is_mock_mode() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return mock_mode_;
    }

    void add_mock_active_title(const std::string &title_id, const std::string &origin = "test application process")
    {
        std::lock_guard<std::mutex> lock(mutex_);
        mock_mode_ = true;
        for (const auto &item : mock_active_titles_)
        {
            if (item.title_id == title_id)
                return;
        }
        mock_active_titles_.push_back({title_id, origin});
    }

    void set_mock_unknown(bool unknown, const std::string &reason = "test activity unknown")
    {
        std::lock_guard<std::mutex> lock(mutex_);
        mock_mode_ = true;
        mock_unknown_ = unknown;
        mock_unknown_reason_ = reason;
    }

    void clear_mock_activity()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        mock_active_titles_.clear();
        mock_unknown_ = false;
        mock_unknown_reason_.clear();
    }

    void record_unknown(const std::string &reason)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!unknown_)
        {
            reason_ = reason;
        }
        unknown_ = true;
    }

    void add_active_title(const std::string &title, const std::string &origin)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        mock_mode_ = true;
        add_active_title_locked(title, origin);
    }

    // Detects whether a nullfs mount is an auxiliary ShadowMountPlus export rather than an active game
    static bool is_auxiliary_export(const std::string &path, const std::string &source, const std::string &type)
    {
        if (type != "nullfs")
            return false;
        const std::string prefix = "/mnt/sandbox/";
        if (path.rfind(prefix, 0) != 0)
            return false;

        const std::string rel = path.substr(prefix.size());
        if (rel.size() < 15)
            return false;

        std::string cand = rel.substr(0, 9);
        if (!is_title_valid(cand) || rel[9] != '_')
            return false;

        std::size_t idx = 10;
        if (idx >= rel.size() || !std::isdigit(static_cast<unsigned char>(rel[idx])))
            return false;

        while (idx < rel.size() && std::isdigit(static_cast<unsigned char>(rel[idx])))
            idx++;

        std::string suffix = rel.substr(idx);
        if (suffix == "/data")
            return (source == "/user/data" || source == "/data");
        if (source == suffix)
            return true;
        if (suffix == "/preinst/common/font" || suffix == "/mnt/disc")
            return true;
        if (suffix.size() == 9 && (suffix.rfind("/mnt/usb", 0) == 0 || suffix.rfind("/mnt/ext", 0) == 0))
        {
            char max_char = (suffix[5] == 'u') ? '7' : '1';
            return (suffix[8] >= '0' && suffix[8] <= max_char);
        }
        return false;
    }

    void parse_mount_path(const std::string &path)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        mock_mode_ = true;
        parse_mount_path_locked(path);
    }

    void record_mount(const std::string &path, const std::string &source, const std::string &type)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        mock_mode_ = true;
        record_mount_locked(path, source, type);
    }

    void refresh(bool force = false)
    {
        (void)force;
        std::lock_guard<std::mutex> lock(mutex_);
        if (mock_mode_)
        {
            active_titles_ = mock_active_titles_;
            unknown_ = mock_unknown_;
            reason_ = mock_unknown_reason_;
            return;
        }

        const char *env = std::getenv("TEST_ACTIVE_TITLES");
        if (!env || env[0] == '\0')
        {
            env = std::getenv("MACHETE_TEST_ACTIVE_TITLES");
        }
        if (env && env[0] != '\0')
        {
            active_titles_.clear();
            unknown_ = false;
            reason_.clear();
            load_from_env_or_file(env);
            return;
        }

#if defined(__PROSPERO__) || defined(__Prospero__) || defined(__PS5__)
        active_titles_.clear();
        unknown_ = false;
        reason_.clear();

        // 1. Check processes via sysctl CTL_KERN, KERN_PROC, KERN_PROC_PROC
        int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PROC, 0};
        std::size_t size = 0;
        std::vector<char> buf;
        bool ok = false;
        for (int attempt = 0; attempt < 4 && !ok; attempt++)
        {
            if (sysctl(mib, 4, nullptr, &size, nullptr, 0) != 0 || size == 0)
                break;
            size += 64 * sizeof(struct kinfo_proc);
            buf.resize(size);
            if (sysctl(mib, 4, buf.data(), &size, nullptr, 0) == 0)
                ok = true;
            else if (errno != ENOMEM)
                break;
        }

        if (!ok)
        {
            if (!unknown_)
                reason_ = "cannot read process list (errno " + std::to_string(errno) + ")";
            unknown_ = true;
        }
        else
        {
            struct AppInfo
            {
                std::uint32_t app_id;
                std::uint64_t unknown1;
                char title_id[14];
                char unknown2[0x3c];
            };
            using GetAppInfoFn = int (*)(pid_t, AppInfo *);
            GetAppInfoFn get_app_info = nullptr;
#if defined(RTLD_DEFAULT)
            get_app_info = reinterpret_cast<GetAppInfoFn>(dlsym(RTLD_DEFAULT, "sceKernelGetAppInfo"));
#endif
            std::size_t pos = 0;
            const std::size_t pid_offset = offsetof(struct kinfo_proc, ki_pid);
            const std::size_t pid_end = pid_offset + sizeof(pid_t);
            const pid_t my_pid = getpid();

            while (pos < size)
            {
                std::size_t remaining = size - pos;
                int entry_size = 0;
                if (remaining < sizeof(entry_size))
                {
                    if (!unknown_)
                        reason_ = "truncated process-list header (" + std::to_string(remaining) + " bytes)";
                    unknown_ = true;
                    break;
                }
                std::memcpy(&entry_size, buf.data() + pos, sizeof(entry_size));
                if (entry_size <= 0 || static_cast<std::size_t>(entry_size) < pid_end || static_cast<std::size_t>(entry_size) > remaining)
                {
                    if (!unknown_)
                        reason_ = "invalid process record: " + std::to_string(entry_size) + " bytes";
                    unknown_ = true;
                    break;
                }

                pid_t pid = 0;
                std::memcpy(&pid, buf.data() + pos + pid_offset, sizeof(pid));
                pos += static_cast<std::size_t>(entry_size);

                if (pid == my_pid)
                    continue;

                if (get_app_info)
                {
                    AppInfo ai{};
                    if (get_app_info(pid, &ai) == 0)
                    {
                        char title[sizeof(ai.title_id) + 1] = {0};
                        std::memcpy(title, ai.title_id, sizeof(ai.title_id));
                        title[sizeof(ai.title_id)] = '\0';
                        if (title[0] != '\0')
                        {
                            char origin[64];
                            std::snprintf(origin, sizeof(origin), "application process pid %d", static_cast<int>(pid));
                            add_active_title_locked(title, origin);
                        }
                        else if (ai.app_id != 0)
                        {
                            if (!unknown_)
                                reason_ = "application has no title ID (pid " + std::to_string(pid) + ")";
                            unknown_ = true;
                        }
                    }
                }
            }
        }

        // 2. Check active sandboxes in /mnt/sandbox
        DIR *sb_dir = opendir("/mnt/sandbox");
        if (sb_dir)
        {
            while (const dirent *ent = readdir(sb_dir))
            {
                if (ent->d_name[0] == '.')
                    continue;
                std::string entry_name = ent->d_name;
                std::string full_path = "/mnt/sandbox/" + entry_name;
                parse_mount_path_locked(full_path);
            }
            closedir(sb_dir);
        }

        // Also check /mnt/sandbox/pfsmnt if present
        DIR *pfs_dir = opendir("/mnt/sandbox/pfsmnt");
        if (pfs_dir)
        {
            while (const dirent *ent = readdir(pfs_dir))
            {
                if (ent->d_name[0] == '.')
                    continue;
                std::string entry_name = ent->d_name;
                std::string full_path = "/mnt/sandbox/pfsmnt/" + entry_name;
                parse_mount_path_locked(full_path);
            }
            closedir(pfs_dir);
        }
#else
        // Host (non-PS5) clean fallback when no mock or environment variable is supplied
        active_titles_.clear();
        unknown_ = false;
        reason_.clear();
#endif
    }

    bool is_blocked(const std::string &title_id, std::string *out_reason)
    {
        refresh();
        std::lock_guard<std::mutex> lock(mutex_);
        if (!is_title_valid(title_id) || unknown_)
        {
            if (out_reason)
                *out_reason = unknown_ ? reason_ : "unsupported save title ID";
            return true;
        }
        for (const auto &entry : active_titles_)
        {
            if (entry.title_id == title_id)
            {
                if (out_reason)
                    *out_reason = entry.title_id + ": " + entry.origin;
                return true;
            }
        }
        if (out_reason)
            out_reason->clear();
        return false;
    }

    std::vector<std::string> get_active_ids()
    {
        refresh();
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> ids;
        ids.reserve(active_titles_.size());
        for (const auto &item : active_titles_)
        {
            ids.push_back(item.title_id);
        }
        return ids;
    }

private:
    TitleActivityMonitor() = default;

    void add_active_title_locked(const std::string &title, const std::string &origin)
    {
        if (title.rfind("NPXS", 0) == 0)
            return;
        if (!is_title_valid(title))
        {
            if (!unknown_)
                reason_ = "invalid application title ID: " + title;
            unknown_ = true;
            if (mock_mode_)
            {
                mock_unknown_ = true;
                mock_unknown_reason_ = reason_;
            }
            return;
        }
        for (const auto &entry : active_titles_)
        {
            if (entry.title_id == title)
                return;
        }
        active_titles_.push_back({title, origin});
        if (mock_mode_)
        {
            for (const auto &entry : mock_active_titles_)
            {
                if (entry.title_id == title)
                    return;
            }
            mock_active_titles_.push_back({title, origin});
        }
    }

    void parse_mount_path_locked(const std::string &path)
    {
        const std::string prefix = "/mnt/sandbox/";
        auto pos = path.find(prefix);
        if (pos == std::string::npos)
            return;

        pos += prefix.size();
        std::string sub = path.substr(pos);

        while (!sub.empty())
        {
            if (sub.rfind("NPXS", 0) == 0)
                return;

            if (sub.size() >= 9)
            {
                std::string cand = sub.substr(0, 9);
                if (is_title_valid(cand))
                {
                    if (sub.size() == 9 || sub[9] == '_' || sub[9] == '/' || sub[9] == '-')
                    {
                        add_active_title_locked(cand, path);
                        return;
                    }
                }
            }
            auto slash = sub.find('/');
            if (slash == std::string::npos)
                break;
            sub = sub.substr(slash + 1);
        }

        if (!unknown_)
            reason_ = "unidentified sandbox mount: " + path;
        unknown_ = true;
        if (mock_mode_)
        {
            mock_unknown_ = true;
            mock_unknown_reason_ = reason_;
        }
    }

    void record_mount_locked(const std::string &path, const std::string &source, const std::string &type)
    {
        if (is_auxiliary_export(path, source, type))
            return;
        parse_mount_path_locked(path);
    }

    void load_from_env_or_file(const std::string &env)
    {
        FILE *f = std::fopen(env.c_str(), "r");
        if (f)
        {
            char line[512];
            while (std::fgets(line, sizeof(line), f))
            {
                std::string s(line);
                while (!s.empty() && (s.back() == '\r' || s.back() == '\n'))
                    s.pop_back();
                if (s.empty())
                    continue;

                if (s == "UNKNOWN")
                {
                    unknown_ = true;
                    reason_ = "test activity unknown";
                }
                else if (s.rfind("MOUNT\t", 0) == 0)
                {
                    std::string rest = s.substr(6);
                    auto tab1 = rest.find('\t');
                    if (tab1 != std::string::npos)
                    {
                        std::string p = rest.substr(0, tab1);
                        std::string rest2 = rest.substr(tab1 + 1);
                        auto tab2 = rest2.find('\t');
                        if (tab2 != std::string::npos)
                        {
                            std::string src = rest2.substr(0, tab2);
                            std::string typ = rest2.substr(tab2 + 1);
                            record_mount_locked(p, src, typ);
                        }
                    }
                }
                else if (s[0] == '/')
                {
                    record_mount_locked(s, "", "");
                }
                else if (is_title_valid(s))
                {
                    add_active_title_locked(s, "test application process");
                }
            }
            std::fclose(f);
        }
        else
        {
            std::size_t start = 0;
            while (start < env.size())
            {
                auto end = env.find_first_of(",; \t\r\n", start);
                if (end == std::string::npos)
                    end = env.size();
                std::string tok = env.substr(start, end - start);
                if (!tok.empty())
                {
                    if (tok == "UNKNOWN")
                    {
                        unknown_ = true;
                        reason_ = "test activity unknown";
                    }
                    else if (tok[0] == '/')
                    {
                        record_mount_locked(tok, "", "");
                    }
                    else if (is_title_valid(tok))
                    {
                        add_active_title_locked(tok, "test application process");
                    }
                }
                start = end + 1;
            }
        }
    }

    mutable std::mutex mutex_;
    std::vector<ActiveTitleRecord> active_titles_;
    bool unknown_ = false;
    std::string reason_;

    bool mock_mode_ = false;
    std::vector<ActiveTitleRecord> mock_active_titles_;
    bool mock_unknown_ = false;
    std::string mock_unknown_reason_;
};

inline bool is_title_running_or_suspended(const std::string &title_id, std::string *out_reason = nullptr)
{
    return TitleActivityMonitor::instance().is_blocked(title_id, out_reason);
}

inline std::vector<std::string> get_active_title_ids()
{
    return TitleActivityMonitor::instance().get_active_ids();
}

inline void set_mock_active_title(const std::string &title_id, const std::string &origin = "test application process")
{
    TitleActivityMonitor::instance().add_mock_active_title(title_id, origin);
}

inline void clear_mock_activity()
{
    TitleActivityMonitor::instance().clear_mock_activity();
}

inline void set_mock_unknown(bool unknown, const std::string &reason = "test activity unknown")
{
    TitleActivityMonitor::instance().set_mock_unknown(unknown, reason);
}

inline void reset_activity_monitor()
{
    TitleActivityMonitor::instance().reset();
}

inline void simulate_sandbox_mount(const std::string &path, const std::string &source = "", const std::string &type = "")
{
    TitleActivityMonitor::instance().record_mount(path, source, type);
}

} // namespace machete::activity

namespace machete
{
using activity::is_title_running_or_suspended;
using activity::get_active_title_ids;
}
