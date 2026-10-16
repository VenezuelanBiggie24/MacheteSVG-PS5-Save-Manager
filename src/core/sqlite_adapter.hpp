// MacheteSVG - SQLite Adapter & Savedata Database Manager
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/logger.hpp"

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

// Localiza las rutas candidatas para savedata.db de un usuario
inline std::vector<std::string> get_savedata_db_candidates(std::uint32_t user_id)
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
    paths.push_back(std::string("/user/home/") + uid_8 + "/savedata.db");
    paths.push_back(std::string("/user/home/") + uid_clean + "/savedata.db");
    paths.push_back(std::string("/user/home/") + uid_8 + "/db/savedata.db");
    paths.push_back(std::string("/user/home/") + uid_clean + "/db/savedata.db");
    paths.push_back("/system_data/priv/mms/savedata.db");
    return paths;
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

// Registra o actualiza un slot de guardado en savedata.db garantizando is_broken = 0 y fake_broken = 0
inline bool update_or_register_ps4_savedata_db(std::uint32_t user_id,
                                               std::uint64_t account_id,
                                               const std::string &title_id,
                                               const std::string &slot_name,
                                               std::uint64_t file_size_bytes)
{
    SqliteAdapter &adapter = SqliteAdapter::instance();
    if (!adapter.is_available())
    {
        MACHETE_LOG("[DB]", "Notice: SQLite not available, skipping savedata.db indexing for %s/%s",
                    title_id.c_str(), slot_name.c_str());
        return false;
    }

    const std::uint32_t blocks = (file_size_bytes > 0) ? static_cast<std::uint32_t>((file_size_bytes + 32767) / 32768) : 32;
    const std::uint32_t size_kib = (file_size_bytes > 0) ? static_cast<std::uint32_t>((file_size_bytes + 1023) / 1024) : 1024;

    const auto candidates = get_savedata_db_candidates(user_id);
    bool any_updated = false;

    for (const auto &dbp : candidates)
    {
        if (access(dbp.c_str(), F_OK) != 0)
            continue;

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

        // 2. Insertar o actualizar la fila correspondiente reseteando explicitamente los flags de corrupcion
        char sql[1024];
        std::snprintf(sql, sizeof(sql),
                      "INSERT OR REPLACE INTO savedata ("
                      "title_id, dir_name, main_title, sub_title, detail, tmp_dir_name, "
                      "is_broken, user_param, blocks, free_blocks, size_kib, mtime, "
                      "fake_broken, account_id, user_id, faked_owner, cloud_icon_url, "
                      "cloud_revision, game_title_id"
                      ") VALUES ("
                      "'%s', '%s', '', '', '', '', "
                      "0, 0, %u, %u, %u, strftime('%%Y-%%m-%%dT%%H:%%M:%%S.00Z', CURRENT_TIMESTAMP), "
                      "0, %llu, %u, 0, '', "
                      "0, '%s');",
                      title_id.c_str(), slot_name.c_str(),
                      blocks, blocks, size_kib,
                      static_cast<unsigned long long>(account_id), user_id,
                      title_id.c_str());

        char *errmsg = nullptr;
        int rc = adapter.exec(db, sql, nullptr, nullptr, &errmsg);
        if (rc == 0)
        {
            // Tambien forzar UPDATE para garantizar que si la fila ya existia se limpien is_broken y fake_broken
            char sql_update[512];
            std::snprintf(sql_update, sizeof(sql_update),
                          "UPDATE savedata SET is_broken = 0, fake_broken = 0, account_id = %llu, user_id = %u "
                          "WHERE title_id = '%s' AND dir_name = '%s';",
                          static_cast<unsigned long long>(account_id), user_id,
                          title_id.c_str(), slot_name.c_str());
            (void)adapter.exec(db, sql_update, nullptr, nullptr, nullptr);

            MACHETE_LOG("[DB]", "Indexed PS4 save in %s: %s/%s (is_broken=0, acc=0x%04llx...%04llx, blocks=%u)",
                        dbp.c_str(), title_id.c_str(), slot_name.c_str(),
                        static_cast<unsigned long long>((account_id >> 48) & 0xffff),
                        static_cast<unsigned long long>(account_id & 0xffff),
                        blocks);
            any_updated = true;
        }
        else
        {
            MACHETE_LOG("[DB]", "Failed to execute savedata insert on %s: %s",
                        dbp.c_str(), errmsg ? errmsg : adapter.errmsg(db));
        }

        if (errmsg)
            adapter.free(errmsg);

        // Forzar vaciado de WAL a disco
        (void)adapter.exec(db, "PRAGMA wal_checkpoint(FULL);", nullptr, nullptr, nullptr);
        adapter.close(db);

        (void)chmod(dbp.c_str(), 0666);
        (void)chmod((dbp + "-wal").c_str(), 0666);
        (void)chmod((dbp + "-shm").c_str(), 0666);
    }

    return any_updated;
}

// Limpia las banderas de corrupcion (is_broken, fake_broken) para todas las entradas de un Title ID
inline bool repair_title_corruption_flags(std::uint32_t user_id, const std::string &title_id)
{
    SqliteAdapter &adapter = SqliteAdapter::instance();
    if (!adapter.is_available())
        return false;

    const auto candidates = get_savedata_db_candidates(user_id);
    bool any_cleared = false;

    for (const auto &dbp : candidates)
    {
        if (access(dbp.c_str(), F_OK) != 0)
            continue;

        void *db = nullptr;
        if (adapter.open(dbp.c_str(), &db) != 0 || !db)
            continue;

        (void)adapter.exec(db, "PRAGMA busy_timeout = 3000;", nullptr, nullptr, nullptr);

        char sql[256];
        std::snprintf(sql, sizeof(sql),
                      "UPDATE savedata SET is_broken = 0, fake_broken = 0 WHERE title_id = '%s';",
                      title_id.c_str());

        int rc = adapter.exec(db, sql, nullptr, nullptr, nullptr);
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
