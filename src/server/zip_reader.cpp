// MacheteSVG - Standalone Streaming ZIP Reader & Extractor
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "server/zip_reader.hpp"
#include "core/logger.hpp"
#include "third_party/puff/puff.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_set>

namespace machete::server
{

namespace
{

inline std::uint16_t read_u16_le(const std::uint8_t *src) noexcept
{
    return static_cast<std::uint16_t>(src[0]) |
           (static_cast<std::uint16_t>(src[1]) << 8);
}

inline std::uint32_t read_u32_le(const std::uint8_t *src) noexcept
{
    return static_cast<std::uint32_t>(src[0]) |
           (static_cast<std::uint32_t>(src[1]) << 8) |
           (static_cast<std::uint32_t>(src[2]) << 16) |
           (static_cast<std::uint32_t>(src[3]) << 24);
}

std::string sanitize_relative_path(std::string_view raw)
{
    std::string out;
    out.reserve(raw.size());
    for (char c : raw)
    {
        if (c == '\\')
        {
            out += '/';
        }
        else
        {
            out += c;
        }
    }
    while (!out.empty() && out.front() == '/')
    {
        out.erase(0, 1);
    }
    // Prevent directory traversal attacks
    std::size_t pos = 0;
    while ((pos = out.find("..", pos)) != std::string::npos)
    {
        if ((pos == 0 || out[pos - 1] == '/') &&
            (pos + 2 == out.size() || out[pos + 2] == '/'))
        {
            out.replace(pos, 2, "__");
        }
        pos += 2;
    }
    return out;
}

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

} // namespace

std::size_t cleanup_obsolete_files(const std::string &target_directory,
                                   const std::vector<std::string> &valid_relative_files,
                                   std::size_t &out_directories_cleaned)
{
    out_directories_cleaned = 0;
    std::unordered_set<std::string> valid_set;
    valid_set.reserve(valid_relative_files.size());
    for (const auto &vf : valid_relative_files)
    {
        valid_set.insert(sanitize_relative_path(vf));
    }

    std::size_t files_removed = 0;

    auto clean_worker = [&](auto &self, const std::string &current_dir, const std::string &rel_prefix, int depth) -> void
    {
        if (depth > 8)
        {
            return;
        }

        ScopedDir dir(opendir(current_dir.c_str()));
        if (!dir.is_valid())
        {
            return;
        }

        std::vector<std::string> subdirs_to_check;
        struct dirent *de = nullptr;
        while ((de = readdir(dir.get())) != nullptr)
        {
            if (std::strcmp(de->d_name, ".") == 0 || std::strcmp(de->d_name, "..") == 0)
            {
                continue;
            }

            std::string full_path = current_dir + "/" + de->d_name;
            std::string rel_path = rel_prefix.empty() ? de->d_name : (rel_prefix + "/" + de->d_name);

            struct stat st{};
            if (stat(full_path.c_str(), &st) != 0)
            {
                continue;
            }

            if (S_ISDIR(st.st_mode))
            {
                subdirs_to_check.push_back(full_path);
                self(self, full_path, rel_path, depth + 1);
            }
            else if (S_ISREG(st.st_mode))
            {
                // Preservar machete.log de la eliminacion fisica (sera truncado limpiamente por reset_fresh)
                if (std::strcmp(de->d_name, "machete.log") == 0)
                {
                    continue;
                }

                if (valid_set.find(rel_path) == valid_set.end())
                {
                    MACHETE_LOG("[CLEANUP]", "Removed obsolete file from previous version: %s", rel_path.c_str());
                    if (unlink(full_path.c_str()) == 0)
                    {
                        files_removed++;
                    }
                }
            }
        }

        dir.reset();

        // Eliminar carpetas que hayan quedado completamente vacias tras depurar archivos obsoletos
        for (const auto &sub : subdirs_to_check)
        {
            if (rmdir(sub.c_str()) == 0)
            {
                out_directories_cleaned++;
                MACHETE_LOG("[CLEANUP]", "Removed empty obsolete directory: %s", sub.c_str());
            }
        }
    };

    clean_worker(clean_worker, target_directory, "", 0);
    return files_removed;
}

bool make_directory_recursive(const std::string &path)
{
    if (path.empty())
    {
        return false;
    }
    std::string current;
    for (std::size_t i = 0; i < path.size(); ++i)
    {
        char c = path[i];
        if (c == '/' || c == '\\')
        {
            if (!current.empty() && current != "." && current != "/")
            {
                struct stat st{};
                if (stat(current.c_str(), &st) != 0)
                {
                    if (mkdir(current.c_str(), 0777) != 0 && errno != EEXIST)
                    {
                        return false;
                    }
                    chmod(current.c_str(), 0777);
                }
            }
        }
        current += c;
    }
    if (!current.empty())
    {
        struct stat st{};
        if (stat(current.c_str(), &st) != 0)
        {
            if (mkdir(current.c_str(), 0777) != 0 && errno != EEXIST)
            {
                return false;
            }
            chmod(current.c_str(), 0777);
        }
    }
    return true;
}

std::string detect_common_root_folder(const std::vector<ZipReaderEntry> &entries)
{
    if (entries.empty())
    {
        return "";
    }

    std::string common_prefix;
    bool first_entry = true;

    for (const auto &entry : entries)
    {
        std::string name = sanitize_relative_path(entry.filename);
        if (name.empty())
        {
            continue;
        }

        std::size_t slash_pos = name.find('/');
        if (slash_pos == std::string::npos)
        {
            // A top-level file exists directly at archive root
            return "";
        }

        std::string top_folder = name.substr(0, slash_pos + 1);
        if (first_entry)
        {
            common_prefix = top_folder;
            first_entry = false;
        }
        else if (top_folder != common_prefix)
        {
            // Entries belong to different top-level folders
            return "";
        }
    }

    return common_prefix;
}

ZipReader::ZipReader() noexcept = default;

ZipReader::~ZipReader() noexcept
{
    close();
}

ZipReader::ZipReader(ZipReader &&other) noexcept
    : fd_(other.fd_), zip_path_(std::move(other.zip_path_)), entries_(std::move(other.entries_))
{
    other.fd_ = -1;
}

ZipReader &ZipReader::operator=(ZipReader &&other) noexcept
{
    if (this != &other)
    {
        close();
        fd_ = other.fd_;
        zip_path_ = std::move(other.zip_path_);
        entries_ = std::move(other.entries_);
        other.fd_ = -1;
    }
    return *this;
}

void ZipReader::close() noexcept
{
    if (fd_ >= 0)
    {
        ::close(fd_);
        fd_ = -1;
    }
    zip_path_.clear();
    entries_.clear();
}

bool ZipReader::open_file(const std::string &zip_path)
{
    close();

    fd_ = open(zip_path.c_str(), O_RDONLY);
    if (fd_ < 0)
    {
        MACHETE_LOG("[ZIP_READER]", "Failed to open ZIP archive: %s (errno: %d)", zip_path.c_str(), errno);
        return false;
    }

    zip_path_ = zip_path;
    if (!parse_archive())
    {
        MACHETE_LOG("[ZIP_READER]", "Failed to parse ZIP archive headers: %s", zip_path.c_str());
        close();
        return false;
    }

    MACHETE_LOG("[ZIP_READER]", "Successfully opened %s with %zu entries",
                zip_path.c_str(), entries_.size());
    return true;
}

std::uint64_t ZipReader::total_uncompressed_size() const noexcept
{
    std::uint64_t total = 0;
    for (const auto &entry : entries_)
    {
        total += entry.uncompressed_size;
    }
    return total;
}

bool ZipReader::has_file(std::string_view name) const noexcept
{
    return find_entry(name) != nullptr;
}

const ZipReaderEntry *ZipReader::find_entry(std::string_view name) const noexcept
{
    for (const auto &entry : entries_)
    {
        std::string clean = sanitize_relative_path(entry.filename);
        if (clean == name)
        {
            return &entry;
        }
        // Match basename or trailing suffix (e.g. "eboot.bin" matching "PPSA99001/eboot.bin")
        if (clean.size() > name.size() &&
            clean[clean.size() - name.size() - 1] == '/' &&
            clean.compare(clean.size() - name.size(), name.size(), name) == 0)
        {
            return &entry;
        }
    }
    return nullptr;
}

bool ZipReader::parse_archive()
{
    if (parse_central_directory())
    {
        return true;
    }
    return parse_local_headers();
}

bool ZipReader::parse_central_directory()
{
    struct stat st{};
    if (fstat(fd_, &st) != 0 || st.st_size < 22)
    {
        return false;
    }

    const std::uint64_t file_size = static_cast<std::uint64_t>(st.st_size);
    const std::size_t max_search_size = std::min<std::uint64_t>(file_size, 65536 + 22);

    std::vector<std::uint8_t> tail_buffer(max_search_size);
    const off_t search_offset = static_cast<off_t>(file_size - max_search_size);

    if (lseek(fd_, search_offset, SEEK_SET) == (off_t)-1)
    {
        return false;
    }

    ssize_t bytes_read = read(fd_, tail_buffer.data(), max_search_size);
    if (bytes_read <= 0 || static_cast<std::size_t>(bytes_read) != max_search_size)
    {
        return false;
    }

    // Locate EOCD signature 0x06054B50 scanning backwards
    off_t eocd_pos = -1;
    for (ssize_t i = static_cast<ssize_t>(max_search_size) - 22; i >= 0; --i)
    {
        if (tail_buffer[static_cast<std::size_t>(i)] == 0x50 &&
            tail_buffer[static_cast<std::size_t>(i + 1)] == 0x4B &&
            tail_buffer[static_cast<std::size_t>(i + 2)] == 0x05 &&
            tail_buffer[static_cast<std::size_t>(i + 3)] == 0x06)
        {
            eocd_pos = search_offset + i;
            break;
        }
    }

    if (eocd_pos < 0)
    {
        return false;
    }

    const std::size_t buf_idx = static_cast<std::size_t>(eocd_pos - search_offset);
    const std::uint8_t *eocd = tail_buffer.data() + buf_idx;

    const std::uint16_t total_entries = read_u16_le(eocd + 10);
    const std::uint32_t cd_size = read_u32_le(eocd + 12);
    const std::uint32_t cd_offset = read_u32_le(eocd + 16);

    if (cd_offset + cd_size > file_size)
    {
        return false;
    }

    if (lseek(fd_, cd_offset, SEEK_SET) == (off_t)-1)
    {
        return false;
    }

    std::vector<std::uint8_t> cd_data(cd_size);
    ssize_t cd_read = read(fd_, cd_data.data(), cd_size);
    if (cd_read <= 0 || static_cast<std::size_t>(cd_read) != cd_size)
    {
        return false;
    }

    entries_.clear();
    entries_.reserve(total_entries);

    std::size_t ptr = 0;
    for (std::uint16_t i = 0; i < total_entries && ptr + 46 <= cd_size; ++i)
    {
        const std::uint8_t *h = cd_data.data() + ptr;
        if (read_u32_le(h) != 0x02014B50U)
        {
            break;
        }

        ZipReaderEntry entry;
        entry.compression_method = read_u16_le(h + 10);
        entry.crc32 = read_u32_le(h + 16);
        entry.compressed_size = read_u32_le(h + 20);
        entry.uncompressed_size = read_u32_le(h + 24);

        const std::uint16_t fname_len = read_u16_le(h + 28);
        const std::uint16_t extra_len = read_u16_le(h + 30);
        const std::uint16_t comment_len = read_u16_le(h + 32);
        const std::uint32_t external_attr = read_u32_le(h + 38);
        entry.local_header_offset = read_u32_le(h + 42);

        ptr += 46;
        if (ptr + fname_len > cd_size)
        {
            break;
        }

        entry.filename.assign(reinterpret_cast<const char *>(cd_data.data() + ptr), fname_len);
        ptr += fname_len + extra_len + comment_len;

        entry.is_directory = (!entry.filename.empty() && (entry.filename.back() == '/' || entry.filename.back() == '\\')) ||
                             ((external_attr & 0x10U) != 0) ||
                             (((external_attr >> 16) & S_IFDIR) == S_IFDIR);

        resolve_data_offset(entry);
        entries_.push_back(std::move(entry));
    }

    return !entries_.empty();
}

bool ZipReader::parse_local_headers()
{
    entries_.clear();
    if (lseek(fd_, 0, SEEK_SET) == (off_t)-1)
    {
        return false;
    }

    std::uint8_t hdr[30];
    while (read(fd_, hdr, sizeof(hdr)) == static_cast<ssize_t>(sizeof(hdr)))
    {
        if (read_u32_le(hdr) != 0x04034B50U)
        {
            // End of local file headers reached (e.g. Central Directory signature 0x02014B50)
            break;
        }

        ZipReaderEntry entry;
        entry.local_header_offset = static_cast<std::uint64_t>(lseek(fd_, 0, SEEK_CUR) - sizeof(hdr));
        entry.compression_method = read_u16_le(hdr + 8);
        entry.crc32 = read_u32_le(hdr + 14);
        entry.compressed_size = read_u32_le(hdr + 18);
        entry.uncompressed_size = read_u32_le(hdr + 22);

        const std::uint16_t fname_len = read_u16_le(hdr + 26);
        const std::uint16_t extra_len = read_u16_le(hdr + 28);

        std::vector<char> fname_buf(fname_len);
        if (read(fd_, fname_buf.data(), fname_len) != static_cast<ssize_t>(fname_len))
        {
            break;
        }
        entry.filename.assign(fname_buf.data(), fname_len);

        if (extra_len > 0)
        {
            if (lseek(fd_, extra_len, SEEK_CUR) == (off_t)-1)
            {
                break;
            }
        }

        entry.data_offset = entry.local_header_offset + 30 + fname_len + extra_len;
        entry.is_directory = (!entry.filename.empty() && (entry.filename.back() == '/' || entry.filename.back() == '\\'));

        entries_.push_back(entry);

        // Skip payload to next local file header
        if (lseek(fd_, static_cast<off_t>(entry.compressed_size), SEEK_CUR) == (off_t)-1)
        {
            break;
        }
    }

    return !entries_.empty();
}

bool ZipReader::resolve_data_offset(ZipReaderEntry &entry)
{
    if (lseek(fd_, static_cast<off_t>(entry.local_header_offset), SEEK_SET) == (off_t)-1)
    {
        return false;
    }

    std::uint8_t lfh[30];
    if (read(fd_, lfh, sizeof(lfh)) != static_cast<ssize_t>(sizeof(lfh)))
    {
        return false;
    }

    if (read_u32_le(lfh) != 0x04034B50U)
    {
        return false;
    }

    const std::uint16_t fname_len = read_u16_le(lfh + 26);
    const std::uint16_t extra_len = read_u16_le(lfh + 28);

    entry.data_offset = entry.local_header_offset + 30 + fname_len + extra_len;
    return true;
}

bool ZipReader::extract_to_memory(const ZipReaderEntry &entry, std::vector<std::uint8_t> &out_data)
{
    if (entry.is_directory || entry.uncompressed_size == 0)
    {
        out_data.clear();
        return true;
    }

    if (lseek(fd_, static_cast<off_t>(entry.data_offset), SEEK_SET) == (off_t)-1)
    {
        return false;
    }

    if (entry.compression_method == 0) // Store
    {
        out_data.resize(static_cast<std::size_t>(entry.uncompressed_size));
        ssize_t r = read(fd_, out_data.data(), out_data.size());
        return (r > 0 && static_cast<std::size_t>(r) == out_data.size());
    }
    else if (entry.compression_method == 8) // Deflate
    {
        std::vector<std::uint8_t> compressed(static_cast<std::size_t>(entry.compressed_size));
        ssize_t r = read(fd_, compressed.data(), compressed.size());
        if (r <= 0 || static_cast<std::size_t>(r) != compressed.size())
        {
            return false;
        }

        out_data.resize(static_cast<std::size_t>(entry.uncompressed_size));
        unsigned long destlen = static_cast<unsigned long>(entry.uncompressed_size);
        unsigned long srclen = static_cast<unsigned long>(entry.compressed_size);

        int ret = puff(out_data.data(), &destlen, compressed.data(), &srclen);
        return (ret == 0 && destlen == entry.uncompressed_size);
    }

    MACHETE_LOG("[ZIP_READER]", "Unsupported compression method %u for %s",
                entry.compression_method, entry.filename.c_str());
    return false;
}

bool ZipReader::extract_to_file(const ZipReaderEntry &entry, const std::string &out_path)
{
    if (entry.is_directory)
    {
        return make_directory_recursive(out_path);
    }

    std::size_t slash = out_path.rfind('/');
    if (slash != std::string::npos)
    {
        make_directory_recursive(out_path.substr(0, slash));
    }

    // Unlink existing file to release open executable inodes and prevent ETXTBUSY on FreeBSD/Prospero
    if (access(out_path.c_str(), F_OK) == 0)
    {
        unlink(out_path.c_str());
    }

    int out_fd = open(out_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (out_fd < 0)
    {
        MACHETE_LOG("[ZIP_READER]", "Failed to open destination file %s (errno: %d)",
                    out_path.c_str(), errno);
        return false;
    }

    bool success = false;
    if (entry.uncompressed_size == 0)
    {
        success = true;
    }
    else if (entry.compression_method == 0) // Store (streamed)
    {
        if (lseek(fd_, static_cast<off_t>(entry.data_offset), SEEK_SET) != (off_t)-1)
        {
            constexpr std::size_t kBufSize = 64 * 1024;
            std::vector<std::uint8_t> buffer(kBufSize);
            std::uint64_t remaining = entry.uncompressed_size;
            success = true;

            while (remaining > 0)
            {
                std::size_t to_read = std::min<std::uint64_t>(remaining, buffer.size());
                ssize_t r = read(fd_, buffer.data(), to_read);
                if (r <= 0)
                {
                    success = false;
                    break;
                }
                ssize_t w = write(out_fd, buffer.data(), static_cast<std::size_t>(r));
                if (w != r)
                {
                    success = false;
                    break;
                }
                remaining -= static_cast<std::uint64_t>(r);
            }
        }
    }
    else if (entry.compression_method == 8) // Deflate
    {
        std::vector<std::uint8_t> uncompressed;
        if (extract_to_memory(entry, uncompressed))
        {
            ssize_t w = write(out_fd, uncompressed.data(), uncompressed.size());
            success = (w == static_cast<ssize_t>(uncompressed.size()));
        }
    }

    fchmod(out_fd, 0777);
    ::close(out_fd);
    chmod(out_path.c_str(), 0777);
    return success;
}

ZipExtractResult ZipReader::extract_all(const std::string &target_directory, bool strip_root_folder, bool clean_install)
{
    ZipExtractResult result;
    if (fd_ < 0 || entries_.empty())
    {
        result.error_message = "Archivo ZIP vacio o no abierto";
        return result;
    }

    if (!make_directory_recursive(target_directory))
    {
        result.error_message = "No se pudo crear el directorio destino: " + target_directory;
        return result;
    }

    std::string root_prefix;
    if (strip_root_folder)
    {
        root_prefix = detect_common_root_folder(entries_);
    }

    std::vector<std::string> valid_relative_files;
    valid_relative_files.reserve(entries_.size());

    for (const auto &entry : entries_)
    {
        std::string rel_name = sanitize_relative_path(entry.filename);
        if (rel_name.empty())
        {
            continue;
        }

        if (!root_prefix.empty() && rel_name.rfind(root_prefix, 0) == 0)
        {
            rel_name.erase(0, root_prefix.size());
        }

        if (rel_name.empty())
        {
            continue;
        }

        std::string dest_path = target_directory + "/" + rel_name;

        if (entry.is_directory)
        {
            if (make_directory_recursive(dest_path))
            {
                result.directories_created++;
            }
        }
        else
        {
            if (!extract_to_file(entry, dest_path))
            {
                result.error_message = "Fallo al extraer el archivo: " + rel_name;
                return result;
            }
            valid_relative_files.push_back(rel_name);
            result.files_extracted++;
            result.total_bytes_extracted += entry.uncompressed_size;
        }
    }

    // Depuracion de archivos obsoletos de versiones anteriores para garantizar un update 100% limpio
    if (clean_install)
    {
        std::size_t dirs_cleaned = 0;
        result.files_cleaned = cleanup_obsolete_files(target_directory, valid_relative_files, dirs_cleaned);
        result.directories_cleaned = dirs_cleaned;
        MACHETE_LOG("[ZIP_READER]", "Clean install complete: %zu files extracted, %zu obsolete files removed, %zu empty folders removed",
                    result.files_extracted, result.files_cleaned, result.directories_cleaned);
    }

    result.success = true;
    return result;
}

} // namespace machete::server
