// MacheteSVG - Streaming ZIP Archive Generator
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace machete::server
{

struct ZipEntry
{
    std::string path_in_zip; // Relative path inside archive (e.g. "PPSA01411/sdimg_slot00")
    std::string disk_path;   // Absolute source path on disk
    std::uint64_t size = 0;
    std::uint32_t crc32 = 0;
    std::uint32_t header_offset = 0;
};

class ZipWriter
{
public:
    ZipWriter() = default;

    // Fast table-driven IEEE 802.3 CRC32 calculation
    static std::uint32_t calculate_crc32(const void *data, std::size_t length, std::uint32_t previous_crc = 0);
    static std::uint32_t file_crc32(const std::string &file_path, std::uint64_t &out_size);

    // Adds a file on disk into the zip with a relative path inside the archive
    void add_file(const std::string &disk_path, const std::string &zip_relative_path);

    // Adds all files in a directory recursively into the zip under a root prefix
    void add_directory(const std::string &dir_path, const std::string &zip_prefix = "");

    // Computes total uncompressed size and pre-calculates CRCs for streaming
    void prepare();

    // Returns total byte length of the resulting ZIP archive (for HTTP Content-Length header)
    std::uint64_t total_zip_size() const;

    // Number of files added to the archive
    std::size_t entry_count() const { return entries_.size(); }

    // Stream writer callback: returns true to continue, false to abort
    using StreamCallback = std::function<bool(const void *data, std::size_t size)>;
    bool stream_to(const StreamCallback &callback) const;

    // Directly streams the entire ZIP archive into a connected socket descriptor
    bool stream_to_fd(int socket_fd) const;

    // Writes entire archive into a memory buffer
    std::vector<std::uint8_t> write_to_memory() const;

private:
    std::vector<ZipEntry> entries_;
    bool prepared_ = false;
};

} // namespace machete::server
