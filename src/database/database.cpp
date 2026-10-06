// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Database Implementation
// ═══════════════════════════════════════════════════════════════════════════════
#include "database/database.h"
#include "core/logger.h"

#ifndef NO_SQLITE
#include <sqlite3.h>
#endif

#include <fstream>
#include <sstream>

namespace ps5dm {

static const char* TAG = "Database";
static const int CURRENT_DB_VERSION = 1;

struct Database::Impl {
#ifndef NO_SQLITE
    sqlite3* db = nullptr;
#endif
    std::string path;
    bool open = false;
    int version = 0;
};

Database& Database::instance() {
    static Database db;
    return db;
}

Database::Database() : impl_(std::make_unique<Impl>()) {}
Database::~Database() { close(); }

Result<void> Database::open(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    impl_->path = path;

#ifndef NO_SQLITE
    int rc = sqlite3_open(path.c_str(), &impl_->db);
    if (rc != SQLITE_OK) {
        std::string err = sqlite3_errmsg(impl_->db);
        sqlite3_close(impl_->db);
        impl_->db = nullptr;
        LOG_ERROR(TAG, "Failed to open database: " + err);
        return Result<void>::failure(Error::make(rc, "Database open failed: " + err));
    }

    // Enable WAL mode for crash safety
    sqlite3_exec(impl_->db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(impl_->db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(impl_->db, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);

    impl_->open = true;
    LOG_INFO(TAG, "Database opened: " + path);

    return createTables();
#else
    // JSON fallback - just mark as open
    impl_->open = true;
    LOG_INFO(TAG, "Database opened (JSON fallback): " + path);
    return Result<void>::success();
#endif
}

void Database::close() {
    std::lock_guard<std::mutex> lock(mutex_);
#ifndef NO_SQLITE
    if (impl_->db) {
        sqlite3_close(impl_->db);
        impl_->db = nullptr;
    }
#endif
    impl_->open = false;
}

Result<void> Database::createTables() {
#ifndef NO_SQLITE
    const char* schema = R"SQL(
        CREATE TABLE IF NOT EXISTS db_version (
            version INTEGER NOT NULL
        );

        CREATE TABLE IF NOT EXISTS downloads (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            url TEXT NOT NULL,
            filename TEXT NOT NULL,
            path TEXT NOT NULL,
            total_size INTEGER DEFAULT -1,
            downloaded_size INTEGER DEFAULT 0,
            status INTEGER DEFAULT 0,
            priority INTEGER DEFAULT 2,
            category INTEGER DEFAULT 0,
            etag TEXT,
            last_modified TEXT,
            content_type TEXT,
            connection_count INTEGER DEFAULT 8,
            retry_count INTEGER DEFAULT 0,
            error_code INTEGER DEFAULT 0,
            error_message TEXT,
            checksum_algorithm INTEGER DEFAULT 0,
            expected_checksum TEXT,
            calculated_checksum TEXT,
            created_at TEXT,
            updated_at TEXT,
            completed_at TEXT
        );

        CREATE TABLE IF NOT EXISTS download_segments (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            download_id INTEGER NOT NULL,
            segment_index INTEGER NOT NULL,
            start_offset INTEGER NOT NULL,
            end_offset INTEGER NOT NULL,
            downloaded_bytes INTEGER DEFAULT 0,
            status INTEGER DEFAULT 0,
            retry_count INTEGER DEFAULT 0,
            FOREIGN KEY (download_id) REFERENCES downloads(id) ON DELETE CASCADE
        );

        CREATE TABLE IF NOT EXISTS archive_groups (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            base_name TEXT NOT NULL,
            archive_type INTEGER DEFAULT 0,
            directory TEXT NOT NULL,
            expected_parts INTEGER DEFAULT 0,
            found_parts INTEGER DEFAULT 0,
            total_size INTEGER DEFAULT 0,
            status INTEGER DEFAULT 0,
            destination TEXT,
            started_at TEXT,
            completed_at TEXT,
            error TEXT
        );

        CREATE TABLE IF NOT EXISTS archive_parts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            group_id INTEGER NOT NULL,
            part_index INTEGER NOT NULL,
            filename TEXT NOT NULL,
            file_path TEXT NOT NULL,
            file_size INTEGER DEFAULT 0,
            found INTEGER DEFAULT 0,
            FOREIGN KEY (group_id) REFERENCES archive_groups(id) ON DELETE CASCADE
        );

        CREATE TABLE IF NOT EXISTS history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            url TEXT,
            filename TEXT NOT NULL,
            path TEXT,
            file_size INTEGER DEFAULT 0,
            duration_sec INTEGER DEFAULT 0,
            avg_speed INTEGER DEFAULT 0,
            status TEXT,
            completed_at TEXT
        );

        CREATE TABLE IF NOT EXISTS settings_kv (
            key TEXT PRIMARY KEY,
            value TEXT
        );

        CREATE INDEX IF NOT EXISTS idx_downloads_status ON downloads(status);
        CREATE INDEX IF NOT EXISTS idx_downloads_category ON downloads(category);
        CREATE INDEX IF NOT EXISTS idx_segments_download ON download_segments(download_id);
        CREATE INDEX IF NOT EXISTS idx_archive_parts_group ON archive_parts(group_id);
    )SQL";

    char* errMsg = nullptr;
    int rc = sqlite3_exec(impl_->db, schema, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::string err = errMsg ? errMsg : "Unknown error";
        sqlite3_free(errMsg);
        LOG_ERROR(TAG, "Failed to create tables: " + err);
        return Result<void>::failure(Error::make(rc, "Schema creation failed: " + err));
    }

    // Check/set version
    sqlite3_stmt* stmt = nullptr;
    rc = sqlite3_prepare_v2(impl_->db, "SELECT version FROM db_version LIMIT 1", -1, &stmt, nullptr);
    if (rc == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
        impl_->version = sqlite3_column_int(stmt, 0);
    } else {
        // Insert initial version
        impl_->version = CURRENT_DB_VERSION;
        std::string insert = "INSERT INTO db_version (version) VALUES (" +
                             std::to_string(CURRENT_DB_VERSION) + ")";
        sqlite3_exec(impl_->db, insert.c_str(), nullptr, nullptr, nullptr);
    }
    if (stmt) sqlite3_finalize(stmt);

    LOG_INFO(TAG, "Database schema ready, version " + std::to_string(impl_->version));
    return Result<void>::success();
#else
    return Result<void>::success();
#endif
}

Result<void> Database::execute(const std::string& sql) {
    std::lock_guard<std::mutex> lock(mutex_);
#ifndef NO_SQLITE
    if (!impl_->db) return Result<void>::failure(Error::make(1, "Database not open"));

    char* errMsg = nullptr;
    int rc = sqlite3_exec(impl_->db, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::string err = errMsg ? errMsg : "Unknown error";
        sqlite3_free(errMsg);
        return Result<void>::failure(Error::make(rc, err));
    }
    return Result<void>::success();
#else
    return Result<void>::success();
#endif
}

Result<void> Database::beginTransaction() {
    return execute("BEGIN TRANSACTION");
}

Result<void> Database::commit() {
    return execute("COMMIT");
}

Result<void> Database::rollback() {
    return execute("ROLLBACK");
}

bool Database::isOpen() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return impl_->open;
}

int Database::version() const {
    return impl_->version;
}

void* Database::handle() const {
#ifndef NO_SQLITE
    return impl_->db;
#else
    return nullptr;
#endif
}

Result<void> Database::migrate() {
    if (impl_->version >= CURRENT_DB_VERSION) {
        return Result<void>::success();
    }
    return runMigration(impl_->version, CURRENT_DB_VERSION);
}

Result<void> Database::runMigration(int fromVersion, int toVersion) {
    LOG_INFO(TAG, "Migrating database from v" + std::to_string(fromVersion) +
             " to v" + std::to_string(toVersion));

    // Future migrations go here
    // if (fromVersion < 2) { ... }

    // Update version
    std::string sql = "UPDATE db_version SET version = " + std::to_string(toVersion);
    auto result = execute(sql);
    if (result.ok()) {
        impl_->version = toVersion;
    }
    return result;
}

} // namespace ps5dm
