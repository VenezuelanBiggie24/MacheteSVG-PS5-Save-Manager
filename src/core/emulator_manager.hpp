// MacheteSVG - Emulator & Homebrew Save Manager
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace machete::emulator
{

struct EmulatorTarget
{
    std::string name;                      // Human-readable name (e.g. "PCSX2 (PS2)")
    std::string id;                        // Special internal title ID (e.g. "EMU_PCSX2")
    std::vector<std::string> source_paths; // List of directories or files where saves reside
    std::string target_backup_subpath;     // Subpath in backup storage (e.g. "emulators/PCSX2")
};

// Returns default registered emulator targets (optionally prefixed with root_prefix)
std::vector<EmulatorTarget> get_known_emulator_targets(const std::string &root_prefix = "");

// Finds a known emulator target by its special internal title ID (e.g. "EMU_PCSX2")
const EmulatorTarget *find_emulator_target_by_id(const std::string &id, const std::string &root_prefix = "");

// Detects installed emulators by checking disk for presence of directories/files
std::vector<EmulatorTarget> detect_installed_emulators(const std::string &root_prefix = "");

// Backs up emulator saves to destination_root/<target_backup_subpath>
bool backup_emulator_saves(const EmulatorTarget &target,
                           const std::string &destination_root,
                           std::string *out_error = nullptr);

// Restores emulator saves from backup_root (or backup_root/<target_backup_subpath>)
bool restore_emulator_saves(const EmulatorTarget &target,
                            const std::string &backup_root,
                            std::string *out_error = nullptr);

// Calculates total byte size of existing save files for target
std::uint64_t calculate_emulator_saves_size(const EmulatorTarget &target);

// Validates whether a path is safe from traversal and belongs to allowable emulator storage (/data)
bool is_safe_emulator_path(const std::string &path);

// Validates whether a backup subpath is safe and relative (no traversal, no absolute path)
bool is_safe_relative_subpath(const std::string &subpath);

} // namespace machete::emulator
