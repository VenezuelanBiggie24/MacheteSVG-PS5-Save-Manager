// MacheteSVG - SQLite Adapter & Savedata Database Manager
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/logger.hpp"
#include "core/sfo_resigner.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "vendor/sqlite/sqlite3.h"

namespace machete::db
{

inline std::string escape_sql_string(const std::string &in)
{
    std::string out;
    out.reserve(in.size() + 8);
    for (char c : in)
    {
        if (c == '\'')
            out += "''";
        else if (c != '\0')
            out += c;
    }
    return out;
}

class SqliteAdapter
{
public:
    static SqliteAdapter &instance()
    {
        static SqliteAdapter s_inst;
        return s_inst;
    }

    bool is_available() const noexcept
    {
        return true;
    }

    int open(const char *filename, void **ppDb)
    {
        if (!filename || !ppDb)
            return -1;
        sqlite3 *db = nullptr;
        int rc = sqlite3_open(filename, &db);
        *ppDb = db;
        return rc;
    }

    int close(void *pDb)
    {
        if (!pDb)
            return 0;
        return sqlite3_close(reinterpret_cast<sqlite3 *>(pDb));
    }

    int exec(void *pDb, const char *sql,
             int (*callback)(void *, int, char **, char **) = nullptr,
             void *arg = nullptr, char **errmsg = nullptr)
    {
        if (!pDb || !sql)
            return -1;
        return sqlite3_exec(reinterpret_cast<sqlite3 *>(pDb), sql, callback, arg, errmsg);
    }

    void free(void *p)
    {
        if (p)
            sqlite3_free(p);
    }

    const char *errmsg(void *pDb)
    {
        if (pDb)
            return sqlite3_errmsg(reinterpret_cast<sqlite3 *>(pDb));
        return "unknown error";
    }

private:
    SqliteAdapter()
    {
        MACHETE_LOG("[DB]", "Embedded SQLite v%s initialized successfully", sqlite3_libversion());
    }

    ~SqliteAdapter() = default;

    SqliteAdapter(const SqliteAdapter &) = delete;
    SqliteAdapter &operator=(const SqliteAdapter &) = delete;
};

// Localiza las rutas candidatas para savedata.db (tanto PS4 como PS5) para consulta de Account ID
inline std::vector<std::string> get_savedata_db_candidates(std::uint32_t user_id)
{
    char uid_8[16];
    std::snprintf(uid_8, sizeof(uid_8), "%08x", user_id);
    char uid_clean[16];
    std::snprintf(uid_clean, sizeof(uid_clean), "%x", user_id);

    std::vector<std::string> paths;
    paths.push_back(std::string("/system_data/savedata/") + uid_8 + "/db/user/savedata.db");
    paths.push_back(std::string("/system_data/savedata/") + uid_clean + "/db/user/savedata.db");
    paths.push_back(std::string("/system_data/savedata_prospero/") + uid_8 + "/db/user/savedata.db");
    paths.push_back(std::string("/system_data/savedata_prospero/") + uid_clean + "/db/user/savedata.db");
    paths.push_back(std::string("/system_data/savedata/") + uid_8 + "/db/savedata.db");
    paths.push_back(std::string("/system_data/savedata/") + uid_clean + "/db/savedata.db");
    paths.push_back(std::string("/system_data/savedata_prospero/") + uid_8 + "/db/savedata.db");
    paths.push_back(std::string("/system_data/savedata_prospero/") + uid_clean + "/db/savedata.db");
    paths.push_back(std::string("/system_data/savedata/") + uid_8 + "/savedata.db");
    paths.push_back(std::string("/system_data/savedata/") + uid_clean + "/savedata.db");
    paths.push_back(std::string("/user/home/") + uid_8 + "/savedata.db");
    paths.push_back(std::string("/user/home/") + uid_clean + "/savedata.db");
    paths.push_back(std::string("/user/home/") + uid_8 + "/db/savedata.db");
    paths.push_back(std::string("/user/home/") + uid_clean + "/db/savedata.db");
    paths.push_back("/system_data/priv/mms/savedata.db");
    return paths;
}

// Localiza las rutas exclusivas de savedata.db para PS4 (CUSA)
inline std::vector<std::string> get_ps4_savedata_db_candidates(std::uint32_t user_id)
{
    char uid_8[16];
    std::snprintf(uid_8, sizeof(uid_8), "%08x", user_id);
    char uid_clean[16];
    std::snprintf(uid_clean, sizeof(uid_clean), "%x", user_id);

    std::vector<std::string> paths;
    paths.push_back(std::string("/system_data/savedata/") + uid_8 + "/db/user/savedata.db");
    paths.push_back(std::string("/system_data/savedata/") + uid_clean + "/db/user/savedata.db");
    paths.push_back(std::string("/system_data/savedata/") + uid_8 + "/db/savedata.db");
    paths.push_back(std::string("/system_data/savedata/") + uid_clean + "/db/savedata.db");
    paths.push_back(std::string("/system_data/savedata/") + uid_8 + "/savedata.db");
    paths.push_back(std::string("/system_data/savedata/") + uid_clean + "/savedata.db");
    return paths;
}

// Localiza las rutas exclusivas de savedata.db para PS5 (PPSA)
inline std::vector<std::string> get_ps5_savedata_db_candidates(std::uint32_t user_id)
{
    char uid_8[16];
    std::snprintf(uid_8, sizeof(uid_8), "%08x", user_id);
    char uid_clean[16];
    std::snprintf(uid_clean, sizeof(uid_clean), "%x", user_id);

    std::vector<std::string> paths;
    paths.push_back(std::string("/system_data/savedata_prospero/") + uid_8 + "/db/user/savedata.db");
    paths.push_back(std::string("/system_data/savedata_prospero/") + uid_clean + "/db/user/savedata.db");
    paths.push_back(std::string("/system_data/savedata_prospero/") + uid_8 + "/db/savedata.db");
    paths.push_back(std::string("/system_data/savedata_prospero/") + uid_clean + "/db/savedata.db");
    return paths;
}

inline std::vector<std::string> filter_existing_unique_dbs(const std::vector<std::string> &candidates)
{
    std::vector<std::string> unique_dbs;
    for (const auto &c : candidates)
    {
        if (access(c.c_str(), F_OK) != 0)
            continue;
        bool dup = false;
        for (const auto &u : unique_dbs)
        {
            if (u == c)
            {
                dup = true;
                break;
            }
        }
        if (!dup)
            unique_dbs.push_back(c);
    }
    return unique_dbs;
}

// Consulta el Account ID registrado en savedata.db para el usuario
inline std::uint64_t query_account_id_from_savedata_db(std::uint32_t user_id)
{
    SqliteAdapter &adapter = SqliteAdapter::instance();
    if (!adapter.is_available())
        return 0;

    const auto candidates = get_savedata_db_candidates(user_id);
    for (const auto &dbp : candidates)
    {
        if (access(dbp.c_str(), R_OK) != 0)
            continue;

        void *db = nullptr;
        if (adapter.open(dbp.c_str(), &db) != 0 || !db)
            continue;

        std::uint64_t found_acc = 0;
        auto cb = [](void *arg, int argc, char **argv, char **) -> int {
            if (argc > 0 && argv && argv[0])
            {
                auto *out = reinterpret_cast<std::uint64_t *>(arg);
                *out = std::strtoull(argv[0], nullptr, 10);
            }
            return 0;
        };

        const char *query = "SELECT account_id FROM savedata WHERE account_id != 0 LIMIT 1;";
        (void)adapter.exec(db, query, cb, &found_acc, nullptr);
        adapter.close(db);

        if (found_acc != 0 && found_acc != ~0ULL)
        {
            MACHETE_LOG("[DB]", "Recovered authentic Account ID from %s: 0x%04llx...%04llx",
                        dbp.c_str(),
                        static_cast<unsigned long long>((found_acc >> 48) & 0xffff),
                        static_cast<unsigned long long>(found_acc & 0xffff));
            return found_acc;
        }
    }
    return 0;
}

// Registra o actualiza un slot de guardado de PS4 en /system_data/savedata/<uid>/db/user/savedata.db
// Purga filas duplicadas historicas (conservando MIN(rowid)) y garantiza is_broken = 0 y fake_broken = 0
inline bool update_or_register_ps4_savedata_db(std::uint32_t user_id,
                                               std::uint64_t account_id,
                                               const std::string &title_id,
                                               const std::string &slot_name,
                                               std::uint64_t file_size_bytes,
                                               const sfo::SfoSaveMetadata *sfo_meta = nullptr,
                                               const std::string &fallback_game_title = "")
{
    SqliteAdapter &adapter = SqliteAdapter::instance();
    if (!adapter.is_available())
    {
        MACHETE_LOG("[DB]", "Notice: SQLite not available, skipping PS4 savedata.db indexing for %s/%s",
                    title_id.c_str(), slot_name.c_str());
        return false;
    }

    std::uint32_t blocks = (file_size_bytes > 0) ? static_cast<std::uint32_t>((file_size_bytes + 32767) / 32768) : 32;
    if (sfo_meta && sfo_meta->blocks > 0)
    {
        blocks = static_cast<std::uint32_t>(sfo_meta->blocks);
    }
    const std::uint32_t size_kib = (file_size_bytes > 0) ? static_cast<std::uint32_t>((file_size_bytes + 1023) / 1024) : (blocks * 32);
    const std::uint32_t user_param = (sfo_meta && sfo_meta->valid) ? sfo_meta->user_param : 0;

    const std::string safe_tid = escape_sql_string(title_id);
    const std::string safe_slot = escape_sql_string(slot_name);
    std::string raw_main = (sfo_meta && sfo_meta->valid && !sfo_meta->main_title.empty())
                               ? sfo_meta->main_title
                               : (!fallback_game_title.empty() ? fallback_game_title : title_id);
    std::string raw_sub = (sfo_meta && sfo_meta->valid && !sfo_meta->sub_title.empty())
                              ? sfo_meta->sub_title
                              : slot_name;
    std::string raw_detail = (sfo_meta && sfo_meta->valid) ? sfo_meta->detail : "";

    const std::string safe_main = escape_sql_string(raw_main);
    const std::string safe_sub = escape_sql_string(raw_sub);
    const std::string safe_detail = escape_sql_string(raw_detail);

    // Purgar cualquier entrada CUSA que se hubiera insertado accidentalmente en savedata_prospero
    const auto ps5_dbs = filter_existing_unique_dbs(get_ps5_savedata_db_candidates(user_id));
    for (const auto &p5db : ps5_dbs)
    {
        void *db5 = nullptr;
        if (adapter.open(p5db.c_str(), &db5) == 0 && db5)
        {
            (void)adapter.exec(db5, "PRAGMA busy_timeout = 3000;", nullptr, nullptr, nullptr);
            (void)adapter.exec(db5, "DELETE FROM savedata WHERE title_id LIKE 'CUSA%';", nullptr, nullptr, nullptr);
            (void)adapter.exec(db5, "PRAGMA wal_checkpoint(FULL);", nullptr, nullptr, nullptr);
            adapter.close(db5);
        }
    }

    const auto unique_dbs = filter_existing_unique_dbs(get_ps4_savedata_db_candidates(user_id));
    bool any_updated = false;

    for (const auto &dbp : unique_dbs)
    {
        MACHETE_LOG("[DB]", "Attempting to index PS4 save in %s: %s/%s", dbp.c_str(), title_id.c_str(), slot_name.c_str());

        void *db = nullptr;
        if (adapter.open(dbp.c_str(), &db) != 0 || !db)
        {
            MACHETE_LOG("[DB]", "Could not open %s for indexing", dbp.c_str());
            continue;
        }

        (void)adapter.exec(db, "PRAGMA busy_timeout = 3000;", nullptr, nullptr, nullptr);

        // 1. Asegurar tabla si no existiera
        const char *create_tbl =
            "CREATE TABLE IF NOT EXISTS savedata ("
            "title_id TEXT, dir_name TEXT, main_title TEXT, sub_title TEXT, detail TEXT, "
            "tmp_dir_name TEXT, is_broken INTEGER, user_param INTEGER, blocks INTEGER, "
            "free_blocks INTEGER, size_kib INTEGER, mtime TEXT, fake_broken INTEGER, "
            "account_id INTEGER, user_id INTEGER, faked_owner INTEGER, cloud_icon_url TEXT, "
            "cloud_revision INTEGER, game_title_id TEXT, PRIMARY KEY (title_id, dir_name));";
        (void)adapter.exec(db, create_tbl, nullptr, nullptr, nullptr);

        // 2. Eliminar filas duplicadas para (title_id, dir_name), conservando siempre MIN(rowid) (la fila original)
        char sql_dedup[512];
        std::snprintf(sql_dedup, sizeof(sql_dedup),
                      "DELETE FROM savedata WHERE title_id = '%s' AND dir_name = '%s' "
                      "AND rowid NOT IN (SELECT MIN(rowid) FROM savedata WHERE title_id = '%s' AND dir_name = '%s');",
                      safe_tid.c_str(), safe_slot.c_str(), safe_tid.c_str(), safe_slot.c_str());
        (void)adapter.exec(db, sql_dedup, nullptr, nullptr, nullptr);

        // 3. Comprobar si la fila existe
        int existing_count = 0;
        auto count_cb = [](void *arg, int argc, char **argv, char **) -> int {
            if (argc > 0 && argv && argv[0])
            {
                *reinterpret_cast<int *>(arg) = std::atoi(argv[0]);
            }
            return 0;
        };
        char sql_check[512];
        std::snprintf(sql_check, sizeof(sql_check),
                      "SELECT COUNT(*) FROM savedata WHERE title_id = '%s' AND dir_name = '%s';",
                      safe_tid.c_str(), safe_slot.c_str());
        (void)adapter.exec(db, sql_check, count_cb, &existing_count, nullptr);

        char *errmsg = nullptr;
        int rc = 0;
        if (existing_count > 0)
        {
            char sql_update[2048];
            if (sfo_meta && sfo_meta->valid && (!sfo_meta->main_title.empty() || !sfo_meta->sub_title.empty() || sfo_meta->user_param != 0))
            {
                std::snprintf(sql_update, sizeof(sql_update),
                              "UPDATE savedata SET is_broken = 0, fake_broken = 0, faked_owner = 0, "
                              "account_id = %llu, user_id = %u, main_title = '%s', sub_title = '%s', "
                              "detail = '%s', user_param = %u, blocks = %u, "
                              "free_blocks = CASE WHEN (free_blocks IS NULL OR free_blocks <= 0) THEN %u ELSE free_blocks END, size_kib = %u, "
                              "mtime = strftime('%%Y-%%m-%%dT%%H:%%M:%%S.00Z', CURRENT_TIMESTAMP) "
                              "WHERE title_id = '%s' AND dir_name = '%s';",
                              static_cast<unsigned long long>(account_id), user_id,
                              safe_main.c_str(), safe_sub.c_str(), safe_detail.c_str(),
                              user_param, blocks, blocks, size_kib,
                              safe_tid.c_str(), safe_slot.c_str());
            }
            else
            {
                // Si la fila existente tenia main_title igual al Title ID crudo, mejorar con el nombre real del juego
                std::snprintf(sql_update, sizeof(sql_update),
                              "UPDATE savedata SET is_broken = 0, fake_broken = 0, faked_owner = 0, "
                              "account_id = %llu, user_id = %u, "
                              "main_title = CASE WHEN (main_title IS NULL OR main_title = '' OR main_title = '%s') THEN '%s' ELSE main_title END, "
                              "blocks = CASE WHEN blocks <= 0 THEN %u ELSE blocks END, "
                              "free_blocks = CASE WHEN (free_blocks IS NULL OR free_blocks <= 0) THEN %u ELSE free_blocks END, "
                              "size_kib = CASE WHEN size_kib <= 0 THEN %u ELSE size_kib END, "
                              "mtime = strftime('%%Y-%%m-%%dT%%H:%%M:%%S.00Z', CURRENT_TIMESTAMP) "
                              "WHERE title_id = '%s' AND dir_name = '%s';",
                              static_cast<unsigned long long>(account_id), user_id,
                              safe_tid.c_str(), safe_main.c_str(),
                              blocks, blocks, size_kib,
                              safe_tid.c_str(), safe_slot.c_str());
            }
            rc = adapter.exec(db, sql_update, nullptr, nullptr, &errmsg);
        }
        else
        {
            char sql_insert[2048];
            std::snprintf(sql_insert, sizeof(sql_insert),
                          "INSERT INTO savedata ("
                          "title_id, dir_name, main_title, sub_title, detail, tmp_dir_name, "
                          "is_broken, user_param, blocks, free_blocks, size_kib, mtime, "
                          "fake_broken, account_id, user_id, faked_owner, cloud_icon_url, "
                          "cloud_revision, game_title_id"
                          ") VALUES ("
                          "'%s', '%s', '%s', '%s', '%s', '', "
                          "0, %u, %u, %u, %u, strftime('%%Y-%%m-%%dT%%H:%%M:%%S.00Z', CURRENT_TIMESTAMP), "
                          "0, %llu, %u, 0, '', "
                          "0, '%s');",
                          safe_tid.c_str(), safe_slot.c_str(),
                          safe_main.c_str(), safe_sub.c_str(), safe_detail.c_str(),
                          user_param, blocks, blocks, size_kib,
                          static_cast<unsigned long long>(account_id), user_id,
                          safe_tid.c_str());
            rc = adapter.exec(db, sql_insert, nullptr, nullptr, &errmsg);
        }

        if (rc == 0)
        {
            MACHETE_LOG("[DB]", "Indexed PS4 save in %s: %s/%s (existing=%d, is_broken=0, acc=0x%04llx...%04llx, blocks=%u, user_param=%u)",
                        dbp.c_str(), title_id.c_str(), slot_name.c_str(), existing_count,
                        static_cast<unsigned long long>((account_id >> 48) & 0xffff),
                        static_cast<unsigned long long>(account_id & 0xffff),
                        blocks, user_param);
            any_updated = true;
        }
        else
        {
            MACHETE_LOG("[DB]", "Failed to execute PS4 savedata query on %s: %s",
                        dbp.c_str(), errmsg ? errmsg : adapter.errmsg(db));
        }

        if (errmsg)
            adapter.free(errmsg);

        (void)adapter.exec(db, "PRAGMA wal_checkpoint(FULL);", nullptr, nullptr, nullptr);
        adapter.close(db);

        (void)chmod(dbp.c_str(), 0666);
        (void)chmod((dbp + "-wal").c_str(), 0666);
        (void)chmod((dbp + "-shm").c_str(), 0666);
    }

    return any_updated;
}

// Registra o actualiza un slot de guardado de PS5 (sdimg_<SLOT> -> dir_name = <SLOT>)
// en /system_data/savedata_prospero/<uid>/db/user/savedata.db para prevenir el error CE-107109-8
inline bool update_or_register_ps5_savedata_db(std::uint32_t user_id,
                                               std::uint64_t account_id,
                                               const std::string &title_id,
                                               const std::string &slot_name,
                                               std::uint64_t file_size_bytes,
                                               const std::string &game_title = "",
                                               const std::string &game_title_id_override = "")
{
    SqliteAdapter &adapter = SqliteAdapter::instance();
    if (!adapter.is_available())
        return false;

    const std::string clean_slot = (slot_name.rfind("sdimg_", 0) == 0) ? slot_name.substr(6) : slot_name;
    if (clean_slot.empty() || clean_slot.rfind("sce_bu_", 0) == 0)
        return false;

    const std::uint32_t size_kib = (file_size_bytes > 0) ? static_cast<std::uint32_t>((file_size_bytes + 1023) / 1024) : 1024;
    const std::uint32_t blocks = (size_kib + 63) / 64; // En PS5 (savedata_prospero) 1 bloque = 64 KiB
    const std::uint32_t system_blocks = (blocks >= 2) ? (blocks / 2) : 1;
    const std::uint32_t free_blocks = (blocks > system_blocks) ? ((blocks - system_blocks) / 2) : 1;

    const std::string safe_tid = escape_sql_string(title_id);
    const std::string safe_gtid = escape_sql_string(game_title_id_override.empty() ? title_id : game_title_id_override);
    const std::string safe_slot = escape_sql_string(clean_slot);
    const std::string safe_main = escape_sql_string(game_title.empty() ? "Datos guardados" : game_title);

    const auto unique_dbs = filter_existing_unique_dbs(get_ps5_savedata_db_candidates(user_id));
    bool any_updated = false;

    for (const auto &dbp : unique_dbs)
    {
        void *db = nullptr;
        if (adapter.open(dbp.c_str(), &db) != 0 || !db)
            continue;

        (void)adapter.exec(db, "PRAGMA busy_timeout = 3000;", nullptr, nullptr, nullptr);

        // Registrar una sola vez en el log una muestra de una fila existente en savedata_prospero para diagnostico de esquema
        static bool s_logged_ps5_sample = false;
        if (!s_logged_ps5_sample)
        {
            s_logged_ps5_sample = true;
            auto sample_cb = [](void *, int argc, char **argv, char **col_names) -> int {
                std::string row_str;
                for (int i = 0; i < argc; ++i)
                {
                    if (i > 0)
                        row_str += ", ";
                    row_str += (col_names && col_names[i]) ? col_names[i] : "?";
                    row_str += "=";
                    row_str += (argv && argv[i]) ? argv[i] : "NULL";
                }
                MACHETE_LOG("[DB]", "PS5 savedata_prospero sample row: {%s}", row_str.c_str());
                return 0;
            };
            (void)adapter.exec(db, "SELECT * FROM savedata WHERE title_id LIKE 'PPSA%' LIMIT 2;", sample_cb, nullptr, nullptr);
        }

        // 1. Eliminar posibles duplicados para (title_id, dir_name)
        char sql_dedup[512];
        std::snprintf(sql_dedup, sizeof(sql_dedup),
                      "DELETE FROM savedata WHERE title_id = '%s' AND dir_name = '%s' "
                      "AND rowid NOT IN (SELECT MIN(rowid) FROM savedata WHERE title_id = '%s' AND dir_name = '%s');",
                      safe_tid.c_str(), safe_slot.c_str(), safe_tid.c_str(), safe_slot.c_str());
        (void)adapter.exec(db, sql_dedup, nullptr, nullptr, nullptr);

        // 2. Comprobar si la fila existe
        int existing_count = 0;
        auto count_cb = [](void *arg, int argc, char **argv, char **) -> int {
            if (argc > 0 && argv && argv[0])
                *reinterpret_cast<int *>(arg) = std::atoi(argv[0]);
            return 0;
        };
        char sql_check[512];
        std::snprintf(sql_check, sizeof(sql_check),
                      "SELECT COUNT(*) FROM savedata WHERE title_id = '%s' AND dir_name = '%s';",
                      safe_tid.c_str(), safe_slot.c_str());
        (void)adapter.exec(db, sql_check, count_cb, &existing_count, nullptr);

        char *errmsg = nullptr;
        int rc = 0;
        if (existing_count > 0)
        {
            char sql_update[1024];
            std::snprintf(sql_update, sizeof(sql_update),
                          "UPDATE savedata SET is_broken = 0, fake_broken = 0, faked_owner = 0, "
                          "account_id = CASE WHEN %llu != 0 THEN %llu ELSE account_id END, "
                          "user_id = CASE WHEN %u != 0 THEN %u ELSE user_id END, "
                          "blocks = CASE WHEN blocks <= 0 THEN %u ELSE blocks END, "
                          "system_blocks = CASE WHEN (system_blocks IS NULL OR system_blocks <= 0) THEN %u ELSE system_blocks END, "
                          "free_blocks = CASE WHEN (free_blocks IS NULL OR free_blocks <= 0) THEN %u ELSE free_blocks END, "
                          "size_kib = CASE WHEN size_kib <= 0 THEN %u ELSE size_kib END, "
                          "game_title_id = '%s', "
                          "convert_performance = 0, account_id_in_usb_path = '0000000000000000', "
                          "mtime = strftime('%%Y-%%m-%%dT%%H:%%M:%%S.00Z', CURRENT_TIMESTAMP) "
                          "WHERE title_id = '%s' AND dir_name = '%s';",
                          static_cast<unsigned long long>(account_id), static_cast<unsigned long long>(account_id),
                          user_id, user_id,
                          blocks, system_blocks, free_blocks, size_kib, safe_gtid.c_str(),
                          safe_tid.c_str(), safe_slot.c_str());
            rc = adapter.exec(db, sql_update, nullptr, nullptr, &errmsg);
        }
        else
        {
            char sql_insert[2048];
            std::snprintf(sql_insert, sizeof(sql_insert),
                          "INSERT INTO savedata ("
                          "title_id, dir_name, main_title, sub_title, detail, tmp_dir_name, "
                          "is_broken, user_param, blocks, free_blocks, size_kib, mtime, "
                          "fake_broken, account_id, user_id, faked_owner, cloud_icon_url, "
                          "cloud_revision, game_title_id, system_blocks, convert_performance, "
                          "account_id_in_usb_path"
                          ") VALUES ("
                          "'%s', '%s', '%s', '%s', '', '', "
                          "0, 0, %u, %u, %u, strftime('%%Y-%%m-%%dT%%H:%%M:%%S.00Z', CURRENT_TIMESTAMP), "
                          "0, %llu, %u, 0, '', "
                          "0, '%s', %u, 0, '0000000000000000');",
                          safe_tid.c_str(), safe_slot.c_str(),
                          safe_main.c_str(), "",
                          blocks, free_blocks, size_kib,
                          static_cast<unsigned long long>(account_id), user_id,
                          safe_gtid.c_str(), system_blocks);
            rc = adapter.exec(db, sql_insert, nullptr, nullptr, &errmsg);
        }

        if (rc == 0)
        {
            MACHETE_LOG("[DB]", "Indexed PS5 save slot in %s: %s/%s (existing=%d, is_broken=0, acc=0x%04llx...%04llx, blocks=%u, sys=%u, free=%u, size_kib=%u, gtid=%s)",
                        dbp.c_str(), title_id.c_str(), clean_slot.c_str(), existing_count,
                        static_cast<unsigned long long>((account_id >> 48) & 0xffff),
                        static_cast<unsigned long long>(account_id & 0xffff),
                        blocks, system_blocks, free_blocks, size_kib, safe_gtid.c_str());
            any_updated = true;
        }
        else
        {
            MACHETE_LOG("[DB]", "Failed to index PS5 save slot in %s: %s",
                        dbp.c_str(), errmsg ? errmsg : adapter.errmsg(db));
        }

        if (errmsg)
            adapter.free(errmsg);

        (void)adapter.exec(db, "PRAGMA wal_checkpoint(FULL);", nullptr, nullptr, nullptr);
        adapter.close(db);

        (void)chmod(dbp.c_str(), 0666);
        (void)chmod((dbp + "-wal").c_str(), 0666);
        (void)chmod((dbp + "-shm").c_str(), 0666);
    }

    return any_updated;
}

// Limpia las banderas de corrupcion (is_broken, fake_broken), deduplica filas y sincroniza account_id/user_id
inline bool repair_title_corruption_flags(std::uint32_t user_id,
                                          const std::string &title_id,
                                          std::uint64_t account_id = 0)
{
    SqliteAdapter &adapter = SqliteAdapter::instance();
    if (!adapter.is_available())
        return false;

    const bool is_ps5 = (title_id.rfind("PPSA", 0) == 0 || title_id.rfind("PPCX", 0) == 0);
    const auto unique_dbs = filter_existing_unique_dbs(
        is_ps5 ? get_ps5_savedata_db_candidates(user_id)
               : get_ps4_savedata_db_candidates(user_id));

    const std::string safe_tid = escape_sql_string(title_id);
    bool any_cleared = false;

    for (const auto &dbp : unique_dbs)
    {
        void *db = nullptr;
        if (adapter.open(dbp.c_str(), &db) != 0 || !db)
            continue;

        (void)adapter.exec(db, "PRAGMA busy_timeout = 3000;", nullptr, nullptr, nullptr);

        // Deduplicar cualquier fila repetida para el mismo (title_id, dir_name) conservando MIN(rowid)
        char sql_dedup[512];
        std::snprintf(sql_dedup, sizeof(sql_dedup),
                      "DELETE FROM savedata WHERE title_id = '%s' "
                      "AND rowid NOT IN (SELECT MIN(rowid) FROM savedata WHERE title_id = '%s' GROUP BY dir_name);",
                      safe_tid.c_str(), safe_tid.c_str());
        (void)adapter.exec(db, sql_dedup, nullptr, nullptr, nullptr);

        // Registrar en log las filas actuales para este Title ID
        auto log_cb = [](void *arg, int argc, char **argv, char **) -> int {
            const char *t_id = reinterpret_cast<const char *>(arg);
            if (argc >= 6 && argv)
            {
                MACHETE_LOG("[DB]", "  Row [%s/%s]: is_broken=%s, fake_broken=%s, acc=%s, uid=%s, blocks=%s, free=%s",
                            t_id ? t_id : "?",
                            argv[0] ? argv[0] : "?",
                            argv[1] ? argv[1] : "?",
                            argv[2] ? argv[2] : "?",
                            argv[3] ? argv[3] : "?",
                            argv[4] ? argv[4] : "?",
                            argv[5] ? argv[5] : "?",
                            (argc >= 7 && argv[6]) ? argv[6] : "?");
            }
            return 0;
        };
        char sql_diag[512];
        std::snprintf(sql_diag, sizeof(sql_diag),
                      "SELECT dir_name, is_broken, fake_broken, account_id, user_id, blocks, free_blocks "
                      "FROM savedata WHERE title_id = '%s';",
                      safe_tid.c_str());
        (void)adapter.exec(db, sql_diag, log_cb, const_cast<char *>(title_id.c_str()), nullptr);

        char sql[512];
        if (account_id != 0)
        {
            std::snprintf(sql, sizeof(sql),
                          "UPDATE savedata SET is_broken = 0, fake_broken = 0, faked_owner = 0, "
                          "account_id = %llu, user_id = %u WHERE title_id = '%s';",
                          static_cast<unsigned long long>(account_id), user_id, safe_tid.c_str());
        }
        else
        {
            std::snprintf(sql, sizeof(sql),
                          "UPDATE savedata SET is_broken = 0, fake_broken = 0, faked_owner = 0 WHERE title_id = '%s';",
                          safe_tid.c_str());
        }

        int rc = adapter.exec(db, sql, nullptr, nullptr, nullptr);
        if (dbp.find("savedata_prospero") != std::string::npos)
        {
            char sql_p5[1024];
            std::snprintf(sql_p5, sizeof(sql_p5),
                          "UPDATE savedata SET "
                          "system_blocks = CASE WHEN (system_blocks IS NULL OR system_blocks <= 0) THEN (CASE WHEN blocks >= 2 THEN blocks / 2 ELSE 1 END) ELSE system_blocks END, "
                          "free_blocks = CASE WHEN (free_blocks IS NULL OR free_blocks <= 0) THEN (CASE WHEN blocks >= 2 THEN blocks / 4 ELSE 1 END) ELSE free_blocks END, "
                          "convert_performance = 0, account_id_in_usb_path = '0000000000000000' "
                          "WHERE title_id = '%s';",
                          safe_tid.c_str());
            (void)adapter.exec(db, sql_p5, nullptr, nullptr, nullptr);
        }
        if (rc == 0)
        {
            MACHETE_LOG("[DB]", "Cleared corruption flags in %s for %s", dbp.c_str(), title_id.c_str());
            any_cleared = true;
        }
        (void)adapter.exec(db, "PRAGMA wal_checkpoint(FULL);", nullptr, nullptr, nullptr);
        adapter.close(db);

        (void)chmod(dbp.c_str(), 0666);
        (void)chmod((dbp + "-wal").c_str(), 0666);
        (void)chmod((dbp + "-shm").c_str(), 0666);
    }
    return any_cleared;
}

} // namespace machete::db
