// MacheteSVG - Standalone Streaming ZIP Reader & Extractor
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace machete::server
{

struct ZipReaderEntry
{
    std::string filename;
    std::uint16_t compression_method = 0; // 0 = Store, 8 = Deflate
    std::uint32_t crc32 = 0;
    std::uint64_t compressed_size = 0;
    std::uint64_t uncompressed_size = 0;
    std::uint64_t local_header_offset = 0;
    std::uint64_t data_offset = 0;
    bool is_directory = false;
};

struct ZipExtractResult
{
    bool success = false;
    std::size_t files_extracted = 0;
    std::size_t directories_created = 0;
    std::uint64_t total_bytes_extracted = 0;
    std::string error_message;
};

class ZipReader
{
public:
    ZipReader() noexcept;
    ~ZipReader() noexcept;

    ZipReader(const ZipReader &) = delete;
    ZipReader &operator=(const ZipReader &) = delete;

    ZipReader(ZipReader &&other) noexcept;
    ZipReader &operator=(ZipReader &&other) noexcept;

    // Opens a ZIP archive file from disk
    bool open_file(const std::string &zip_path);

    // Closes the archive and releases file descriptors
    void close() noexcept;

    // Check if an archive is open and parsed
    bool is_open() const noexcept { return fd_ >= 0; }

    // List of entries discovered in the archive
    const std::vector<ZipReaderEntry> &entries() const noexcept { return entries_; }

    // Number of entries in archive
    std::size_t entry_count() const noexcept { return entries_.size(); }

    // Total uncompressed payload size of all files in archive
    std::uint64_t total_uncompressed_size() const noexcept;

    // Checks if the archive contains an entry with given filename (exact or suffix match)
    bool has_file(std::string_view name) const noexcept;

    // Finds an entry by name
    const ZipReaderEntry *find_entry(std::string_view name) const noexcept;

    // Extracts a single entry to a memory buffer
    bool extract_to_memory(const ZipReaderEntry &entry, std::vector<std::uint8_t> &out_data);

    // Extracts a single entry directly to a file on disk
    bool extract_to_file(const ZipReaderEntry &entry, const std::string &out_path);

    // Extracts all files into target_directory, replacing existing files and applying chmod 0777
    // If strip_root_folder is true, strips leading common directory (e.g. "PPSA99001/")
    ZipExtractResult extract_all(const std::string &target_directory, bool strip_root_folder = true);

private:
    int fd_ = -1;
    std::string zip_path_;
    std::vector<ZipReaderEntry> entries_;

    bool parse_archive();
    bool parse_central_directory();
    bool parse_local_headers();
    bool resolve_data_offset(ZipReaderEntry &entry);
};

// Helper to recursively create directories with 0777 permissions
bool make_directory_recursive(const std::string &path);

// Helper to determine common top directory among entries (e.g. "PPSA99001/")
std::string detect_common_root_folder(const std::vector<ZipReaderEntry> &entries);

} // namespace machete::server
