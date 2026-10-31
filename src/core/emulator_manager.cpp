// MacheteSVG - Emulator & Homebrew Save Manager Implementation
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/emulator_manager.hpp"
#include "core/logger.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace machete::emulator
{

namespace
{

std::string clean_prefix(const std::string &prefix)
{
    std::string p = prefix;
    while (!p.empty() && p.back() == '/')
    {
        p.pop_back();
    }
    return p;
}

std::string apply_prefix(const std::string &prefix, const std::string &path)
{
    if (prefix.empty())
        return path;

    std::string p = clean_prefix(prefix);
    if (!path.empty() && path.front() == '/')
        return p + path;

    return p + "/" + path;
}

std::string extract_tail(const std::string &path)
{
    if (path.empty())
        return "";
    std::string p = path;
    while (p.size() > 1 && p.back() == '/')
        p.pop_back();

    std::size_t slash = p.find_last_of('/');
    if (slash != std::string::npos)
        return p.substr(slash + 1);

    return p;
}

bool ensure_dir(const std::string &dir_path)
{
    if (dir_path.empty())
        return true;

    struct stat st{};
    if (::stat(dir_path.c_str(), &st) == 0)
    {
        return S_ISDIR(st.st_mode);
    }

    std::string path_acc;
    std::size_t start = 0;
    if (dir_path.front() == '/')
    {
        path_acc = "/";
        start = 1;
    }

    while (start < dir_path.size())
    {
        std::size_t next = dir_path.find('/', start);
        std::string component = (next == std::string::npos)
                                    ? dir_path.substr(start)
                                    : dir_path.substr(start, next - start);

        if (!component.empty())
        {
            if (path_acc.empty() || path_acc.back() != '/')
                path_acc += "/";
            path_acc += component;

            struct stat sub_st{};
            if (::stat(path_acc.c_str(), &sub_st) != 0)
            {
                if (::mkdir(path_acc.c_str(), 0777) != 0)
                {
                    if (::stat(path_acc.c_str(), &sub_st) != 0 || !S_ISDIR(sub_st.st_mode))
                    {
                        return false;
                    }
                }
            }
        }

        if (next == std::string::npos)
            break;
        start = next + 1;
    }

    return true;
}

bool copy_single_file(const std::string &src, const std::string &dst)
{
    std::size_t slash = dst.find_last_of('/');
    if (slash != std::string::npos)
    {
        ensure_dir(dst.substr(0, slash));
    }

    FILE *in = std::fopen(src.c_str(), "rb");
    if (!in)
        return false;

    FILE *out = std::fopen(dst.c_str(), "wb");
    if (!out)
    {
        std::fclose(in);
        return false;
    }

    char buffer[65536];
    std::size_t bytes_read = 0;
    bool write_ok = true;
    while ((bytes_read = std::fread(buffer, 1, sizeof(buffer), in)) > 0)
    {
        if (std::fwrite(buffer, 1, bytes_read, out) != bytes_read)
        {
            write_ok = false;
            break;
        }
    }

    std::fflush(out);
    int fd = fileno(out);
    if (fd >= 0)
    {
        ::fsync(fd);
    }

    std::fclose(out);
    std::fclose(in);

    if (write_ok)
    {
        ::chmod(dst.c_str(), 0666);
    }
    return write_ok;
}

bool copy_dir_recursive(const std::string &src_dir, const std::string &dst_dir, int &copied_count)
{
    if (!ensure_dir(dst_dir))
        return false;

    DIR *d = ::opendir(src_dir.c_str());
    if (!d)
        return false;

    bool all_ok = true;
    while (const dirent *ent = ::readdir(d))
    {
        if (std::strcmp(ent->d_name, ".") == 0 || std::strcmp(ent->d_name, "..") == 0)
            continue;

        std::string s_item = src_dir + "/" + ent->d_name;
        std::string d_item = dst_dir + "/" + ent->d_name;

        struct stat st{};
        if (::stat(s_item.c_str(), &st) != 0)
            continue;

        if (S_ISDIR(st.st_mode))
        {
            if (!copy_dir_recursive(s_item, d_item, copied_count))
                all_ok = false;
        }
        else if (S_ISREG(st.st_mode))
        {
            if (copy_single_file(s_item, d_item))
            {
                copied_count++;
            }
            else
            {
                all_ok = false;
            }
        }
    }
    ::closedir(d);
    return all_ok;
}

std::uint64_t calculate_directory_size_recursive(const std::string &dir_path)
{
    std::uint64_t total = 0;
    DIR *d = ::opendir(dir_path.c_str());
    if (!d)
        return 0;

    while (const dirent *ent = ::readdir(d))
    {
        if (std::strcmp(ent->d_name, ".") == 0 || std::strcmp(ent->d_name, "..") == 0)
            continue;

        std::string item_path = dir_path + "/" + ent->d_name;
        struct stat st{};
        if (::stat(item_path.c_str(), &st) == 0)
        {
            if (S_ISREG(st.st_mode))
            {
                total += static_cast<std::uint64_t>(st.st_size);
            }
            else if (S_ISDIR(st.st_mode))
            {
                total += calculate_directory_size_recursive(item_path);
            }
        }
    }
    ::closedir(d);
    return total;
}

} // namespace

std::vector<EmulatorTarget> get_known_emulator_targets(const std::string &root_prefix)
{
    std::vector<EmulatorTarget> targets;

    // 1. PCSX2 (PS2)
    {
        EmulatorTarget t;
        t.name = "PCSX2 (PS2)";
        t.id = "EMU_PCSX2";
        t.target_backup_subpath = "emulators/PCSX2";
        t.source_paths = {
            apply_prefix(root_prefix, "/data/PCSX2/Mcd001.ps2"),
            apply_prefix(root_prefix, "/data/PCSX2/Mcd002.ps2"),
            apply_prefix(root_prefix, "/data/PCSX2/memcards"),
            apply_prefix(root_prefix, "/data/PCSX2/sstates"),
        };
        targets.push_back(std::move(t));
    }

    // 2. Porpoise (GameCube / Wii)
    {
        EmulatorTarget t;
        t.name = "Porpoise (GameCube/Wii)";
        t.id = "EMU_PORPOISE";
        t.target_backup_subpath = "emulators/Porpoise";
        t.source_paths = {
            apply_prefix(root_prefix, "/data/porpoise/saves"),
        };
        targets.push_back(std::move(t));
    }

    // 3. RPCS3 (PS3)
    {
        EmulatorTarget t;
        t.name = "RPCS3 (PS3)";
        t.id = "EMU_RPCS3";
        t.target_backup_subpath = "emulators/RPCS3";
        t.source_paths = {
            apply_prefix(root_prefix, "/data/homebrew/PPSA99303/rpcs3/dev_hdd0/home"),
            apply_prefix(root_prefix, "/data/homebrew/PPSA99303/rpcs3/dev_hdd0/savedata"),
        };
        targets.push_back(std::move(t));
    }

    // 4. ProsperoEden (Switch)
    {
        EmulatorTarget t;
        t.name = "ProsperoEden (Switch)";
        t.id = "EMU_EDEN";
        t.target_backup_subpath = "emulators/ProsperoEden";
        t.source_paths = {
            apply_prefix(root_prefix, "/data/prosperoeden/user/nand"),
        };
        targets.push_back(std::move(t));
    }

    // 5. PS5X360 (Xbox 360)
    {
        EmulatorTarget t;
        t.name = "PS5X360 (Xbox 360)";
        t.id = "EMU_PS5X360";
        t.target_backup_subpath = "emulators/PS5X360";
        t.source_paths = {
            apply_prefix(root_prefix, "/data/homebrew/PPSA50011/saves"),
        };
        targets.push_back(std::move(t));
    }

    return targets;
}

const EmulatorTarget *find_emulator_target_by_id(const std::string &id, const std::string &root_prefix)
{
    static std::vector<EmulatorTarget> s_cached_targets;
    s_cached_targets = get_known_emulator_targets(root_prefix);
    for (const auto &t : s_cached_targets)
    {
        if (t.id == id)
            return &t;
    }
    return nullptr;
}

std::vector<EmulatorTarget> detect_installed_emulators(const std::string &root_prefix)
{
    std::vector<EmulatorTarget> detected;
    const auto known = get_known_emulator_targets(root_prefix);

    for (const auto &target : known)
    {
        std::vector<std::string> existing_paths;
        for (const auto &p : target.source_paths)
        {
            struct stat st{};
            if (::stat(p.c_str(), &st) == 0)
            {
                existing_paths.push_back(p);
            }
        }

        // Special check for PCSX2: if /data/PCSX2 directory exists, scan for any extra .ps2 files
        if (target.id == "EMU_PCSX2")
        {
            std::string pcsx2_dir = apply_prefix(root_prefix, "/data/PCSX2");
            struct stat st{};
            if (::stat(pcsx2_dir.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
            {
                DIR *d = ::opendir(pcsx2_dir.c_str());
                if (d)
                {
                    while (const dirent *entry = ::readdir(d))
                    {
                        if (entry->d_name[0] == '.')
                            continue;
                        std::string fn = entry->d_name;
                        if (fn.size() > 4 && fn.substr(fn.size() - 4) == ".ps2")
                        {
                            std::string full = pcsx2_dir + "/" + fn;
                            if (std::find(existing_paths.begin(), existing_paths.end(), full) == existing_paths.end())
                            {
                                existing_paths.push_back(full);
                            }
                        }
                    }
                    ::closedir(d);
                }
            }
        }

        if (!existing_paths.empty())
        {
            EmulatorTarget found = target;
            found.source_paths = existing_paths;
            MACHETE_LOG("[EMULATOR]", "Detected emulator %s (%s) with %zu active path(s)",
                        found.name.c_str(), found.id.c_str(), found.source_paths.size());
            detected.push_back(std::move(found));
        }
    }

    return detected;
}

bool backup_emulator_saves(const EmulatorTarget &target,
                           const std::string &destination_root,
                           std::string *out_error)
{
    if (destination_root.empty())
    {
        if (out_error)
            *out_error = "Destination root is empty";
        return false;
    }

    std::string dest_base = destination_root;
    while (!dest_base.empty() && dest_base.back() == '/')
        dest_base.pop_back();

    std::string target_dest = dest_base + "/" + target.target_backup_subpath;
    if (!ensure_dir(target_dest))
    {
        if (out_error)
            *out_error = "Failed to create destination directory: " + target_dest;
        return false;
    }

    int total_copied = 0;
    std::vector<std::pair<std::string, std::string>> manifest_entries;

    for (const auto &src : target.source_paths)
    {
        struct stat st{};
        if (::stat(src.c_str(), &st) != 0)
            continue;

        std::string tail = extract_tail(src);
        if (tail.empty())
            continue;

        if (S_ISREG(st.st_mode))
        {
            std::string dst_file = target_dest + "/" + tail;
            if (copy_single_file(src, dst_file))
            {
                total_copied++;
                manifest_entries.emplace_back(tail, src);
            }
        }
        else if (S_ISDIR(st.st_mode))
        {
            std::string dst_subdir = target_dest + "/" + tail;
            int dir_copied = 0;
            if (copy_dir_recursive(src, dst_subdir, dir_copied))
            {
                total_copied += dir_copied;
                manifest_entries.emplace_back(tail, src);
            }
        }
    }

    if (total_copied == 0)
    {
        if (out_error)
            *out_error = "No save files found to backup for emulator: " + target.name;
        return false;
    }

    // Write manifest
    std::string manifest_path = target_dest + "/.machete_emulator";
    FILE *mf = std::fopen(manifest_path.c_str(), "w");
    if (mf)
    {
        std::fprintf(mf, "id=%s\nname=%s\nsubpath=%s\n",
                     target.id.c_str(), target.name.c_str(), target.target_backup_subpath.c_str());
        for (const auto &entry : manifest_entries)
        {
            std::fprintf(mf, "entry:%s=%s\n", entry.first.c_str(), entry.second.c_str());
        }
        std::fclose(mf);
        ::chmod(manifest_path.c_str(), 0666);
    }

    MACHETE_LOG("[EMULATOR]", "Backed up %d items for %s into %s",
                total_copied, target.name.c_str(), target_dest.c_str());

    ::sync();
    return true;
}

bool restore_emulator_saves(const EmulatorTarget &target,
                            const std::string &backup_root,
                            std::string *out_error)
{
    if (backup_root.empty())
    {
        if (out_error)
            *out_error = "Backup root is empty";
        return false;
    }

    std::string root = backup_root;
    while (!root.empty() && root.back() == '/')
        root.pop_back();

    std::string backup_dir = root + "/" + target.target_backup_subpath;
    struct stat st_sub{};
    if (::stat(backup_dir.c_str(), &st_sub) != 0 || !S_ISDIR(st_sub.st_mode))
    {
        struct stat st_root{};
        if (::stat(root.c_str(), &st_root) == 0 && S_ISDIR(st_root.st_mode))
        {
            backup_dir = root;
        }
        else
        {
            if (out_error)
                *out_error = "Backup directory not found: " + backup_dir;
            return false;
        }
    }

    int total_restored = 0;

    // Check for manifest
    std::string manifest_path = backup_dir + "/.machete_emulator";
    std::vector<std::pair<std::string, std::string>> manifest_entries;
    FILE *mf = std::fopen(manifest_path.c_str(), "r");
    if (mf)
    {
        char line[1024];
        while (std::fgets(line, sizeof(line), mf))
        {
            std::string l = line;
            while (!l.empty() && (l.back() == '\n' || l.back() == '\r'))
                l.pop_back();

            if (l.rfind("entry:", 0) == 0)
            {
                std::string entry = l.substr(6);
                std::size_t eq = entry.find('=');
                if (eq != std::string::npos)
                {
                    std::string rel = entry.substr(0, eq);
                    std::string orig = entry.substr(eq + 1);
                    manifest_entries.emplace_back(rel, orig);
                }
            }
        }
        std::fclose(mf);
    }

    if (!manifest_entries.empty())
    {
        for (const auto &me : manifest_entries)
        {
            const std::string &rel_name = me.first;
            std::string src_item = backup_dir + "/" + rel_name;
            struct stat item_st{};
            if (::stat(src_item.c_str(), &item_st) != 0)
                continue;

            // Find matching destination path in target.source_paths (to respect test prefixes or custom configs)
            std::string dest_path = me.second;
            for (const auto &sp : target.source_paths)
            {
                std::string sp_tail = extract_tail(sp);
                if (sp_tail == rel_name)
                {
                    dest_path = sp;
                    break;
                }
            }

            if (S_ISREG(item_st.st_mode))
            {
                if (copy_single_file(src_item, dest_path))
                    total_restored++;
            }
            else if (S_ISDIR(item_st.st_mode))
            {
                int d_copied = 0;
                if (copy_dir_recursive(src_item, dest_path, d_copied))
                    total_restored += d_copied;
            }
        }
    }
    else
    {
        // No manifest: match against target.source_paths
        for (const auto &dest_path : target.source_paths)
        {
            std::string tail = extract_tail(dest_path);
            if (tail.empty())
                continue;

            std::string src_candidate = backup_dir + "/" + tail;
            struct stat item_st{};
            if (::stat(src_candidate.c_str(), &item_st) == 0)
            {
                if (S_ISREG(item_st.st_mode))
                {
                    if (copy_single_file(src_candidate, dest_path))
                        total_restored++;
                }
                else if (S_ISDIR(item_st.st_mode))
                {
                    int d_copied = 0;
                    if (copy_dir_recursive(src_candidate, dest_path, d_copied))
                        total_restored += d_copied;
                }
            }
            else if (target.source_paths.size() == 1)
            {
                // Single-directory target where backup_dir directly holds save files
                int d_copied = 0;
                if (copy_dir_recursive(backup_dir, dest_path, d_copied) && d_copied > 0)
                    total_restored += d_copied;
            }
        }
    }

    if (total_restored == 0)
    {
        if (out_error)
            *out_error = "No save files could be restored for emulator: " + target.name;
        return false;
    }

    MACHETE_LOG("[EMULATOR]", "Restored %d items for %s from %s",
                total_restored, target.name.c_str(), backup_dir.c_str());

    ::sync();
    return true;
}

std::uint64_t calculate_emulator_saves_size(const EmulatorTarget &target)
{
    std::uint64_t total = 0;
    for (const auto &src : target.source_paths)
    {
        struct stat st{};
        if (::stat(src.c_str(), &st) != 0)
            continue;

        if (S_ISREG(st.st_mode))
        {
            total += static_cast<std::uint64_t>(st.st_size);
        }
        else if (S_ISDIR(st.st_mode))
        {
            total += calculate_directory_size_recursive(src);
        }
    }
    return total;
}

} // namespace machete::emulator
