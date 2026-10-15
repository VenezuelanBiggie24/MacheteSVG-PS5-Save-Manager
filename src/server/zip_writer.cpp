// MacheteSVG - Streaming ZIP Archive Generator
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "server/zip_writer.hpp"
#include "core/logger.hpp"

#include <array>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

namespace machete::server
{

namespace
{

constexpr std::array<std::uint32_t, 256> make_crc_table()
{
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t i = 0; i < 256; ++i)
    {
        std::uint32_t c = i;
        for (int j = 0; j < 8; ++j)
        {
            c = (c & 1) ? (0xEDB88320U ^ (c >> 1)) : (c >> 1);
        }
        table[i] = c;
    }
    return table;
}

constexpr auto kCrcTable = make_crc_table();

inline void write_u16_le(std::uint8_t *dst, std::uint16_t val)
{
    dst[0] = static_cast<std::uint8_t>(val & 0xFF);
    dst[1] = static_cast<std::uint8_t>((val >> 8) & 0xFF);
}

inline void write_u32_le(std::uint8_t *dst, std::uint32_t val)
{
    dst[0] = static_cast<std::uint8_t>(val & 0xFF);
    dst[1] = static_cast<std::uint8_t>((val >> 8) & 0xFF);
    dst[2] = static_cast<std::uint8_t>((val >> 16) & 0xFF);
    dst[3] = static_cast<std::uint8_t>((val >> 24) & 0xFF);
}

} // namespace

std::uint32_t ZipWriter::calculate_crc32(const void *data, std::size_t length, std::uint32_t previous_crc)
{
    std::uint32_t crc = ~previous_crc;
    const auto *bytes = static_cast<const std::uint8_t *>(data);
    for (std::size_t i = 0; i < length; ++i)
    {
        crc = kCrcTable[(crc ^ bytes[i]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

std::uint32_t ZipWriter::file_crc32(const std::string &file_path, std::uint64_t &out_size)
{
    out_size = 0;
    int fd = open(file_path.c_str(), O_RDONLY);
    if (fd < 0)
    {
        return 0;
    }

    std::uint32_t crc = 0;
    std::vector<std::uint8_t> buffer(64 * 1024);
    while (true)
    {
        ssize_t bytes_read = read(fd, buffer.data(), buffer.size());
        if (bytes_read <= 0)
        {
            break;
        }
        crc = calculate_crc32(buffer.data(), static_cast<std::size_t>(bytes_read), crc);
        out_size += static_cast<std::uint64_t>(bytes_read);
    }
    close(fd);
    return crc;
}

void ZipWriter::add_file(const std::string &disk_path, const std::string &zip_relative_path)
{
    ZipEntry entry;
    entry.disk_path = disk_path;
    entry.path_in_zip = zip_relative_path;

    struct stat st;
    if (stat(disk_path.c_str(), &st) == 0 && S_ISREG(st.st_mode))
    {
        entry.size = static_cast<std::uint64_t>(st.st_size);
        entries_.push_back(std::move(entry));
        prepared_ = false;
    }
}

void ZipWriter::add_directory(const std::string &dir_path, const std::string &zip_prefix)
{
    DIR *dir = opendir(dir_path.c_str());
    if (!dir)
    {
        return;
    }

    struct dirent *de;
    while ((de = readdir(dir)) != nullptr)
    {
        if (std::strcmp(de->d_name, ".") == 0 || std::strcmp(de->d_name, "..") == 0)
        {
            continue;
        }

        std::string full_path = dir_path + "/" + de->d_name;
        std::string rel_path = zip_prefix.empty() ? de->d_name : (zip_prefix + "/" + de->d_name);

        struct stat st;
        if (stat(full_path.c_str(), &st) == 0)
        {
            if (S_ISDIR(st.st_mode))
            {
                add_directory(full_path, rel_path);
            }
            else if (S_ISREG(st.st_mode))
            {
                add_file(full_path, rel_path);
            }
        }
    }
    closedir(dir);
}

void ZipWriter::prepare()
{
    std::uint32_t current_offset = 0;
    for (auto &entry : entries_)
    {
        entry.crc32 = file_crc32(entry.disk_path, entry.size);
        entry.header_offset = current_offset;

        // Local header: 30 bytes + name length + file size
        const std::uint32_t name_len = static_cast<std::uint32_t>(entry.path_in_zip.size());
        current_offset += 30 + name_len + static_cast<std::uint32_t>(entry.size);
    }
    prepared_ = true;
}

std::uint64_t ZipWriter::total_zip_size() const
{
    std::uint64_t total = 0;
    // Local file headers + payload
    for (const auto &entry : entries_)
    {
        total += 30 + entry.path_in_zip.size() + entry.size;
    }
    // Central directory headers
    for (const auto &entry : entries_)
    {
        total += 46 + entry.path_in_zip.size();
    }
    // End of central directory record
    total += 22;
    return total;
}

bool ZipWriter::stream_to(const StreamCallback &callback) const
{
    if (entries_.empty())
    {
        return false;
    }

    std::uint32_t cd_offset = 0;
    std::vector<std::uint8_t> buffer(64 * 1024);

    // 1. Emit all local headers and file contents
    for (const auto &entry : entries_)
    {
        std::uint8_t local_header[30];
        write_u32_le(local_header + 0, 0x04034B50U); // Local file header signature
        write_u16_le(local_header + 4, 20);          // Version needed: 2.0
        write_u16_le(local_header + 6, 0);           // General purpose bit flag
        write_u16_le(local_header + 8, 0);           // Compression method: 0 (Store)
        write_u16_le(local_header + 10, 0x5421);     // DOS time
        write_u16_le(local_header + 12, 0x5421);     // DOS date
        write_u32_le(local_header + 14, entry.crc32);
        write_u32_le(local_header + 18, static_cast<std::uint32_t>(entry.size));
        write_u32_le(local_header + 22, static_cast<std::uint32_t>(entry.size));
        write_u16_le(local_header + 26, static_cast<std::uint16_t>(entry.path_in_zip.size()));
        write_u16_le(local_header + 28, 0); // Extra field length

        if (!callback(local_header, sizeof(local_header)))
        {
            return false;
        }
        if (!callback(entry.path_in_zip.data(), entry.path_in_zip.size()))
        {
            return false;
        }

        cd_offset += 30 + static_cast<std::uint32_t>(entry.path_in_zip.size());

        // Stream file contents
        int fd = open(entry.disk_path.c_str(), O_RDONLY);
        if (fd >= 0)
        {
            std::uint64_t bytes_left = entry.size;
            while (bytes_left > 0)
            {
                std::size_t chunk_size = std::min<std::uint64_t>(bytes_left, buffer.size());
                ssize_t read_bytes = read(fd, buffer.data(), chunk_size);
                if (read_bytes <= 0)
                {
                    break;
                }
                if (!callback(buffer.data(), static_cast<std::size_t>(read_bytes)))
                {
                    close(fd);
                    return false;
                }
                bytes_left -= static_cast<std::uint64_t>(read_bytes);
            }
            close(fd);
        }
        cd_offset += static_cast<std::uint32_t>(entry.size);
    }

    // 2. Emit Central Directory Records
    const std::uint32_t cd_start_offset = cd_offset;
    for (const auto &entry : entries_)
    {
        std::uint8_t cd_header[46];
        write_u32_le(cd_header + 0, 0x02014B50U); // Central file header signature
        write_u16_le(cd_header + 4, 20);          // Version made by: 2.0
        write_u16_le(cd_header + 6, 20);          // Version needed: 2.0
        write_u16_le(cd_header + 8, 0);           // General purpose bit flag
        write_u16_le(cd_header + 10, 0);          // Compression method: 0 (Store)
        write_u16_le(cd_header + 12, 0x5421);     // DOS time
        write_u16_le(cd_header + 14, 0x5421);     // DOS date
        write_u32_le(cd_header + 16, entry.crc32);
        write_u32_le(cd_header + 20, static_cast<std::uint32_t>(entry.size));
        write_u32_le(cd_header + 24, static_cast<std::uint32_t>(entry.size));
        write_u16_le(cd_header + 28, static_cast<std::uint16_t>(entry.path_in_zip.size()));
        write_u16_le(cd_header + 30, 0); // Extra field length
        write_u16_le(cd_header + 32, 0); // File comment length
        write_u16_le(cd_header + 34, 0); // Disk number start
        write_u16_le(cd_header + 36, 0); // Internal attributes
        write_u32_le(cd_header + 38, 0); // External attributes
        write_u32_le(cd_header + 42, entry.header_offset);

        if (!callback(cd_header, sizeof(cd_header)))
        {
            return false;
        }
        if (!callback(entry.path_in_zip.data(), entry.path_in_zip.size()))
        {
            return false;
        }

        cd_offset += 46 + static_cast<std::uint32_t>(entry.path_in_zip.size());
    }

    const std::uint32_t cd_size = cd_offset - cd_start_offset;

    // 3. Emit End of Central Directory Record (EOCD)
    std::uint8_t eocd[22];
    write_u32_le(eocd + 0, 0x06054B50U); // EOCD signature
    write_u16_le(eocd + 4, 0);           // Number of this disk
    write_u16_le(eocd + 6, 0);           // Disk where central directory starts
    write_u16_le(eocd + 8, static_cast<std::uint16_t>(entries_.size()));
    write_u16_le(eocd + 10, static_cast<std::uint16_t>(entries_.size()));
    write_u32_le(eocd + 12, cd_size);
    write_u32_le(eocd + 16, cd_start_offset);
    write_u16_le(eocd + 20, 0); // Comment length

    return callback(eocd, sizeof(eocd));
}

bool ZipWriter::stream_to_fd(int socket_fd) const
{
    return stream_to([socket_fd](const void *data, std::size_t size) -> bool {
        const auto *ptr = static_cast<const char *>(data);
        std::size_t remaining = size;
        while (remaining > 0)
        {
            ssize_t sent = write(socket_fd, ptr, remaining);
            if (sent <= 0)
            {
                return false;
            }
            ptr += sent;
            remaining -= static_cast<std::size_t>(sent);
        }
        return true;
    });
}

std::vector<std::uint8_t> ZipWriter::write_to_memory() const
{
    std::vector<std::uint8_t> result;
    result.reserve(static_cast<std::size_t>(total_zip_size()));
    stream_to([&result](const void *data, std::size_t size) -> bool {
        const auto *bytes = static_cast<const std::uint8_t *>(data);
        result.insert(result.end(), bytes, bytes + size);
        return true;
    });
    return result;
}

} // namespace machete::server
