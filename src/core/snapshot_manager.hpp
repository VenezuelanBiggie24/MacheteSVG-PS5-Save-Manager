// MacheteSVG - Snapshot & Backup Rotation Engine
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/logger.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <vector>

namespace machete::snapshot
{

constexpr std::size_t DEFAULT_MAX_SNAPSHOTS_TO_KEEP = 3;

struct SnapshotInfo
{
    std::string date_str;
    std::string full_path;
    uint64_t total_size_bytes = 0;
    time_t created_at = 0;
};

struct FileSignature
{
    int64_t size = 0;
    int64_t mtime_sec = 0;
    int64_t mtime_nsec = 0;
    int64_t ctime_sec = 0;
    int64_t ctime_nsec = 0;
    uint64_t inode = 0;
    uint64_t device = 0;
};

inline std::atomic<std::size_t>& get_retention_limit_atomic()
{
    static std::atomic<std::size_t> s_max_snapshots{DEFAULT_MAX_SNAPSHOTS_TO_KEEP};
    return s_max_snapshots;
}

inline std::atomic<bool>& get_snapshots_enabled_atomic()
{
    static std::atomic<bool> s_snapshots_enabled{true};
    return s_snapshots_enabled;
}

inline std::size_t get_snapshots_to_keep()
{
    return get_retention_limit_atomic().load();
}

inline void set_snapshots_to_keep(std::size_t max_keep)
{
    get_retention_limit_atomic().store(max_keep);
}

inline bool are_snapshots_enabled()
{
    return get_snapshots_enabled_atomic().load();
}

inline void set_snapshots_enabled(bool enabled)
{
    get_snapshots_enabled_atomic().store(enabled);
}

inline std::string get_date_str_from_time(std::time_t t)
{
    static std::mutex s_time_mutex;
    std::lock_guard<std::mutex> lock(s_time_mutex);
    const std::tm *lt = std::localtime(&t);
    if (lt)
    {
        char buf[16] = {0};
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
                      lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday);
        return std::string(buf);
    }
    return "";
}

inline std::string get_current_date_str()
{
    const std::time_t now = std::time(nullptr);
    return get_date_str_from_time(now);
}

inline bool is_leap_year(int year) noexcept
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

inline int days_in_month(int year, int month) noexcept
{
    switch (month)
    {
        case 1:  return 31;
        case 2:  return is_leap_year(year) ? 29 : 28;
        case 3:  return 31;
        case 4:  return 30;
        case 5:  return 31;
        case 6:  return 30;
        case 7:  return 31;
        case 8:  return 31;
        case 9:  return 30;
        case 10: return 31;
        case 11: return 30;
        case 12: return 31;
        default: return 0;
    }
}

inline bool is_valid_date_str(const std::string& s)
{
    if (s.size() != 10)
        return false;
    for (std::size_t i = 0; i < 10; ++i)
    {
        if (i == 4 || i == 7)
        {
            if (s[i] != '-')
                return false;
        }
        else
        {
            if (s[i] < '0' || s[i] > '9')
                return false;
        }
    }
    const int year = (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 + (s[3] - '0');
    const int month = (s[5] - '0') * 10 + (s[6] - '0');
    const int day = (s[8] - '0') * 10 + (s[9] - '0');
    if (year < 1970 || year > 2099 || month < 1 || month > 12)
        return false;

    const int max_days = days_in_month(year, month);
    if (day < 1 || day > max_days)
        return false;

    return true;
}

inline bool is_temporary_or_internal_file(const char* name) noexcept
{
    if (!name || name[0] == '\0')
        return true;
    if (std::strcmp(name, ".") == 0 || std::strcmp(name, "..") == 0)
        return true;
    if (std::strcmp(name, ".image_stamp") == 0 ||
        std::strcmp(name, ".ps5save-index") == 0 ||
        std::strcmp(name, "snapshots") == 0 ||
        std::strcmp(name, ".DS_Store") == 0)
        return true;

    const std::size_t len = std::strlen(name);
    if (len >= 5 && std::strcmp(name + len - 5, ".part") == 0)
        return true;
    if (len >= 4 && std::strcmp(name + len - 4, ".tmp") == 0)
        return true;
    if (len >= 5 && std::strcmp(name + len - 5, ".temp") == 0)
        return true;
    if (len > 0 && name[len - 1] == '~')
        return true;

    return false;
}

inline bool is_date_part_name(const std::string& s)
{
    if (s.size() != 15)
        return false;
    if (s.compare(10, 5, ".part") != 0)
        return false;
    return is_valid_date_str(s.substr(0, 10));
}

inline bool remove_dir_tree(const std::string& path)
{
    struct stat st{};
    if (lstat(path.c_str(), &st) != 0)
    {
        return (errno == ENOENT);
    }
    if (!S_ISDIR(st.st_mode))
    {
        return (unlink(path.c_str()) == 0);
    }

    DIR* dir = opendir(path.c_str());
    if (!dir)
        return false;

    bool ok = true;
    while (const dirent* dp = readdir(dir))
    {
        if (std::strcmp(dp->d_name, ".") == 0 || std::strcmp(dp->d_name, "..") == 0)
            continue;

        const std::string sub = path + "/" + dp->d_name;
        if (!remove_dir_tree(sub))
            ok = false;
    }
    closedir(dir);

    if (rmdir(path.c_str()) != 0)
        ok = false;

    return ok;
}

inline bool ensure_directories(const std::string& path)
{
    if (path.empty())
        return false;

    struct stat st{};
    if (stat(path.c_str(), &st) == 0)
        return S_ISDIR(st.st_mode);

    std::string current;
    for (std::size_t i = 0; i < path.size(); ++i)
    {
        current += path[i];
        if (path[i] == '/' || i == path.size() - 1)
        {
            if (current == "/" || current.empty())
                continue;
            if (stat(current.c_str(), &st) != 0)
            {
                if (mkdir(current.c_str(), 0777) != 0 && errno != EEXIST)
                    return false;
            }
            else if (!S_ISDIR(st.st_mode))
            {
                return false;
            }
        }
    }
    return true;
}

inline uint64_t calculate_snapshot_size(const std::string& path)
{
    struct stat st{};
    if (stat(path.c_str(), &st) != 0)
        return 0;

    if (S_ISREG(st.st_mode))
        return static_cast<uint64_t>(st.st_size);

    if (!S_ISDIR(st.st_mode))
        return 0;

    DIR* dir = opendir(path.c_str());
    if (!dir)
        return 0;

    uint64_t total = 0;
    while (const dirent* dp = readdir(dir))
    {
        if (std::strcmp(dp->d_name, ".") == 0 || std::strcmp(dp->d_name, "..") == 0)
            continue;

        const std::string sub = path + "/" + dp->d_name;
        struct stat sub_st{};
        if (lstat(sub.c_str(), &sub_st) == 0)
        {
            if (S_ISDIR(sub_st.st_mode))
                total += calculate_snapshot_size(sub);
            else if (S_ISREG(sub_st.st_mode))
                total += static_cast<uint64_t>(sub_st.st_size);
        }
    }
    closedir(dir);
    return total;
}

inline bool copy_tree_for_snapshot(const std::string& src, const std::string& dst, uint64_t& out_bytes, int depth = 0)
{
    if (depth > 16)
        return false;

    if (!ensure_directories(dst))
        return false;

    DIR* dir = opendir(src.c_str());
    if (!dir)
        return false;

    bool ok = true;
    while (const dirent* dp = readdir(dir))
    {
        if (is_temporary_or_internal_file(dp->d_name))
        {
            continue;
        }

        const std::string spath = src + "/" + dp->d_name;
        const std::string dpath = dst + "/" + dp->d_name;

        struct stat st{};
        if (lstat(spath.c_str(), &st) != 0)
        {
            ok = false;
            break;
        }

        if (S_ISDIR(st.st_mode))
        {
            if (!copy_tree_for_snapshot(spath, dpath, out_bytes, depth + 1))
            {
                ok = false;
                break;
            }
        }
        else if (S_ISREG(st.st_mode))
        {
            int in_fd = open(spath.c_str(), O_RDONLY);
            if (in_fd < 0)
            {
                ok = false;
                break;
            }

            int out_fd = open(dpath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (out_fd < 0)
            {
                close(in_fd);
                ok = false;
                break;
            }

            constexpr std::size_t kBufSize = 65536;
            char buf[kBufSize];
            ssize_t n = 0;
            bool copy_ok = true;

            while ((n = read(in_fd, buf, sizeof(buf))) > 0)
            {
                char* p = buf;
                while (n > 0)
                {
                    ssize_t w = write(out_fd, p, n);
                    if (w < 0)
                    {
                        if (errno == EINTR)
                            continue;
                        copy_ok = false;
                        break;
                    }
                    p += w;
                    n -= w;
                }
                if (!copy_ok)
                    break;
            }
            if (n < 0)
                copy_ok = false;

            close(in_fd);
            if (close(out_fd) != 0)
                copy_ok = false;

            if (copy_ok)
            {
                struct timeval tv[2];
                tv[0].tv_sec = st.st_mtime;
                tv[0].tv_usec = 0;
                tv[1] = tv[0];
                utimes(dpath.c_str(), tv);

                out_bytes += static_cast<uint64_t>(st.st_size);
            }
            else
            {
                ok = false;
                break;
            }
        }
    }

    closedir(dir);
    return ok;
}

inline bool compute_signature_recursive(const std::string& path, FileSignature& sig, int depth = 0)
{
    if (depth > 16)
        return false;

    struct stat st{};
    if (stat(path.c_str(), &st) != 0)
        return false;

    if (S_ISREG(st.st_mode))
    {
        sig.size += st.st_size;
        int64_t nsec = 0;
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__PROSPERO__) || defined(__Prospero__) || defined(__PS5__)
        nsec = st.st_mtimespec.tv_nsec;
#else
        nsec = st.st_mtim.tv_nsec;
#endif
        if (st.st_mtime > sig.mtime_sec || (st.st_mtime == sig.mtime_sec && nsec > sig.mtime_nsec))
        {
            sig.mtime_sec = st.st_mtime;
            sig.mtime_nsec = nsec;
        }
        if (sig.inode == 0)
        {
            sig.inode = st.st_ino;
            sig.device = st.st_dev;
        }
        return true;
    }

    if (!S_ISDIR(st.st_mode))
        return false;

    if (sig.inode == 0)
    {
        sig.inode = st.st_ino;
        sig.device = st.st_dev;
    }

    DIR* dir = opendir(path.c_str());
    if (!dir)
        return false;

    while (const dirent* dp = readdir(dir))
    {
        if (is_temporary_or_internal_file(dp->d_name))
            continue;

        const std::string child = path + "/" + dp->d_name;
        compute_signature_recursive(child, sig, depth + 1);
    }
    closedir(dir);
    return true;
}

inline bool compute_save_signature(const std::string& live_save_path, FileSignature& out_sig)
{
    std::memset(&out_sig, 0, sizeof(out_sig));
    return compute_signature_recursive(live_save_path, out_sig, 0);
}

inline bool is_save_modified_since_last_backup(const std::string& live_save_path, const std::string& backup_dest_dir)
{
    FileSignature current_sig{};
    if (!compute_save_signature(live_save_path, current_sig))
    {
        return false;
    }

    const std::string stamp_path = backup_dest_dir + "/.image_stamp";
    std::FILE* f = std::fopen(stamp_path.c_str(), "rb");
    if (!f)
    {
        // No stamp found, backup is needed
        return true;
    }

    char magic[8] = {0};
    FileSignature stored_sig{};
    const bool ok = (std::fread(magic, 1, sizeof(magic), f) == sizeof(magic)) &&
                    (std::memcmp(magic, "PS5IMG2\n", 8) == 0) &&
                    (std::fread(&stored_sig, 1, sizeof(stored_sig), f) == sizeof(stored_sig));
    std::fclose(f);

    if (!ok)
    {
        // Invalid or corrupted stamp -> modified
        return true;
    }

    if (stored_sig.size != current_sig.size ||
        stored_sig.mtime_sec != current_sig.mtime_sec ||
        stored_sig.mtime_nsec != current_sig.mtime_nsec)
    {
        return true;
    }

    return false;
}

inline void update_save_signature_stamp(const std::string& live_save_path, const std::string& backup_dest_dir)
{
    FileSignature sig{};
    if (!compute_save_signature(live_save_path, sig))
    {
        return;
    }

    ensure_directories(backup_dest_dir);

    const std::string stamp_path = backup_dest_dir + "/.image_stamp";
    const std::string part_path = stamp_path + ".part";

    std::FILE* f = std::fopen(part_path.c_str(), "wb");
    if (!f)
    {
        return;
    }

    const char magic[8] = {'P', 'S', '5', 'I', 'M', 'G', '2', '\n'};
    bool ok = (std::fwrite(magic, 1, sizeof(magic), f) == sizeof(magic)) &&
              (std::fwrite(&sig, 1, sizeof(sig), f) == sizeof(sig));

    if (std::fflush(f) != 0 || ::fsync(fileno(f)) != 0)
    {
        ok = false;
    }

    if (std::fclose(f) != 0)
    {
        ok = false;
    }

    if (!ok || std::rename(part_path.c_str(), stamp_path.c_str()) != 0)
    {
        (void)unlink(part_path.c_str());
    }
}

inline bool create_dated_snapshot(const std::string& live_backup_dir,
                                  const std::string& snapshots_dir,
                                  const std::string& date_str,
                                  std::string* out_error = nullptr)
{
    struct stat st_live{};
    if (stat(live_backup_dir.c_str(), &st_live) != 0 || !S_ISDIR(st_live.st_mode))
    {
        if (out_error)
            *out_error = "Live backup directory does not exist or is not a directory: " + live_backup_dir;
        return false;
    }

    if (!is_valid_date_str(date_str))
    {
        if (out_error)
            *out_error = "Invalid date string format (expected YYYY-MM-DD): " + date_str;
        return false;
    }

    if (!ensure_directories(snapshots_dir))
    {
        if (out_error)
            *out_error = "Failed to create snapshots directory: " + snapshots_dir;
        return false;
    }

    const std::string final_dir = snapshots_dir + "/" + date_str;
    struct stat st_final{};
    if (stat(final_dir.c_str(), &st_final) == 0)
    {
        if (out_error)
            *out_error = "Snapshot for date already exists: " + date_str;
        return false;
    }

    const std::string part_dir = snapshots_dir + "/" + date_str + ".part";
    remove_dir_tree(part_dir);

    if (!ensure_directories(part_dir))
    {
        if (out_error)
            *out_error = "Failed to create temporary snapshot directory: " + part_dir;
        return false;
    }

    uint64_t bytes_copied = 0;
    if (!copy_tree_for_snapshot(live_backup_dir, part_dir, bytes_copied))
    {
        remove_dir_tree(part_dir);
        if (out_error)
            *out_error = "Failed while copying files from live backup to snapshot";
        return false;
    }

    ::sync();

    if (std::rename(part_dir.c_str(), final_dir.c_str()) != 0)
    {
        remove_dir_tree(part_dir);
        if (out_error)
            *out_error = "Failed to rename snapshot from temporary folder: " + std::string(std::strerror(errno));
        return false;
    }

    return true;
}

inline size_t prune_old_snapshots(const std::string& snapshots_dir, size_t max_snapshots_to_keep)
{
    DIR* dir = opendir(snapshots_dir.c_str());
    if (!dir)
        return 0;

    std::vector<std::string> valid_dates;
    std::vector<std::string> stale_parts;

    while (const dirent* dp = readdir(dir))
    {
        if (std::strcmp(dp->d_name, ".") == 0 || std::strcmp(dp->d_name, "..") == 0)
            continue;

        const std::string name = dp->d_name;
        if (is_date_part_name(name))
        {
            stale_parts.push_back(snapshots_dir + "/" + name);
        }
        else if (is_valid_date_str(name))
        {
            const std::string full = snapshots_dir + "/" + name;
            struct stat st{};
            if (stat(full.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
            {
                valid_dates.push_back(name);
            }
        }
    }
    closedir(dir);

    // Clean up stale interrupted part folders
    for (const auto& stale : stale_parts)
    {
        remove_dir_tree(stale);
    }

    // Sort descending: newest date first (e.g. 2026-10-31 before 2026-10-30)
    std::sort(valid_dates.begin(), valid_dates.end(), std::greater<std::string>());

    // Do not prune if retention is not exceeded. If retention quota is 0, preserve
    // at least the single latest snapshot so the only existing backup is not lost.
    if (max_snapshots_to_keep == 0)
    {
        max_snapshots_to_keep = 1;
    }

    size_t pruned_count = 0;
    if (valid_dates.size() > max_snapshots_to_keep)
    {
        for (size_t i = max_snapshots_to_keep; i < valid_dates.size(); ++i)
        {
            const std::string to_remove = snapshots_dir + "/" + valid_dates[i];
            if (remove_dir_tree(to_remove))
            {
                pruned_count++;
            }
        }
    }

    return pruned_count;
}

inline std::vector<SnapshotInfo> list_snapshots(const std::string& base_backup_dir)
{
    std::vector<SnapshotInfo> result;
    if (base_backup_dir.empty())
        return result;

    std::string snap_dir = base_backup_dir;

    bool has_direct = false;
    DIR* d = opendir(base_backup_dir.c_str());
    if (d)
    {
        while (const dirent* ent = readdir(d))
        {
            if (is_valid_date_str(ent->d_name))
            {
                has_direct = true;
                break;
            }
        }
        closedir(d);
    }

    if (!has_direct)
    {
        const std::string cand1 = base_backup_dir + "/snapshots";
        struct stat st1{};
        if (stat(cand1.c_str(), &st1) == 0 && S_ISDIR(st1.st_mode))
        {
            snap_dir = cand1;
        }
        else
        {
            const std::string cand2 = base_backup_dir + "/backup_daily";
            struct stat st2{};
            if (stat(cand2.c_str(), &st2) == 0 && S_ISDIR(st2.st_mode))
            {
                snap_dir = cand2;
            }
        }
    }

    DIR* dir = opendir(snap_dir.c_str());
    if (!dir)
        return result;

    while (const dirent* dp = readdir(dir))
    {
        if (std::strcmp(dp->d_name, ".") == 0 || std::strcmp(dp->d_name, "..") == 0)
            continue;

        const std::string name = dp->d_name;
        if (!is_valid_date_str(name))
            continue;

        const std::string full = snap_dir + "/" + name;
        struct stat st{};
        if (stat(full.c_str(), &st) != 0 || !S_ISDIR(st.st_mode))
            continue;

        SnapshotInfo info;
        info.date_str = name;
        info.full_path = full;
        info.total_size_bytes = calculate_snapshot_size(full);
        info.created_at = st.st_mtime;
        result.push_back(std::move(info));
    }
    closedir(dir);

    // Sort by date descending (newest first)
    std::sort(result.begin(), result.end(), [](const SnapshotInfo& a, const SnapshotInfo& b) {
        return a.date_str > b.date_str;
    });

    return result;
}

} // namespace machete::snapshot
