#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Database
// ═══════════════════════════════════════════════════════════════════════════════
#include "common/types.h"
#include <string>
#include <vector>
#include <mutex>
#include <functional>
#include <memory>

namespace ps5dm {

/// Database abstraction layer (SQLite backend when available, JSON fallback)
class Database {
public:
    static Database& instance();

    /// Open/create database
    Result<void> open(const std::string& path);

    /// Close database
    void close();

    /// Run migrations
    Result<void> migrate();

    /// Execute raw SQL (for SQLite backend)
    Result<void> execute(const std::string& sql);

    /// Begin transaction
    Result<void> beginTransaction();

    /// Commit transaction
    Result<void> commit();

    /// Rollback transaction
    Result<void> rollback();

    /// Check if database is open
    bool isOpen() const;

    /// Get database version
    int version() const;

    /// Get internal handle (sqlite3* or nullptr)
    void* handle() const;

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

private:
    Database();
    ~Database();

    Result<void> createTables();
    Result<void> runMigration(int fromVersion, int toVersion);

    struct Impl;
    std::unique_ptr<Impl> impl_;
    mutable std::mutex mutex_;
};

} // namespace ps5dm
