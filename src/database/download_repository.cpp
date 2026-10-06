// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Download Repository Implementation
// ═══════════════════════════════════════════════════════════════════════════════
#include "database/download_repository.h"
#include "database/database.h"
#include "core/logger.h"

#ifndef NO_SQLITE
#include <sqlite3.h>
#endif

namespace ps5dm {

static const char* TAG = "DownloadRepo";

DownloadRepository& DownloadRepository::instance() {
    static DownloadRepository repo;
    return repo;
}

Result<void> DownloadRepository::save(const DownloadInfo& info) {
#ifndef NO_SQLITE
    auto* db = static_cast<sqlite3*>(Database::instance().handle());
    if (!db) return Result<void>::failure(Error::make(1, "Database not open"));

    const char* sql = R"SQL(
        INSERT OR REPLACE INTO downloads
        (id, url, filename, path, total_size, downloaded_size, status, priority,
         category, etag, last_modified, content_type, connection_count,
         retry_count, error_code, error_message, checksum_algorithm,
         expected_checksum, calculated_checksum, created_at, updated_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )SQL";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<void>::failure(Error::make(rc, sqlite3_errmsg(db)));
    }

    sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(info.id));
    sqlite3_bind_text(stmt, 2, info.url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, info.filename.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, info.savePath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 5, info.totalSize);
    sqlite3_bind_int64(stmt, 6, info.downloadedSize.load());
    sqlite3_bind_int(stmt, 7, static_cast<int>(info.status));
    sqlite3_bind_int(stmt, 8, static_cast<int>(info.priority));
    sqlite3_bind_int(stmt, 9, static_cast<int>(info.category));
    sqlite3_bind_text(stmt, 10, info.etag.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 11, info.lastModified.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 12, info.contentType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 13, info.connectionCount);
    sqlite3_bind_int(stmt, 14, info.retryCount);
    sqlite3_bind_int(stmt, 15, info.lastError.code);
    sqlite3_bind_text(stmt, 16, info.lastError.message.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 17, static_cast<int>(info.checksumAlgorithm));
    sqlite3_bind_text(stmt, 18, info.expectedChecksum.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 19, info.calculatedChecksum.c_str(), -1, SQLITE_TRANSIENT);

    auto createdStr = formatTimestamp(info.createdAt);
    auto updatedStr = formatTimestamp(info.updatedAt);
    sqlite3_bind_text(stmt, 20, createdStr.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 21, updatedStr.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return Result<void>::failure(Error::make(rc, sqlite3_errmsg(db)));
    }

    return Result<void>::success();
#else
    LOG_DEBUG(TAG, "Save download (no-op, no SQLite)");
    return Result<void>::success();
#endif
}

Result<void> DownloadRepository::updateProgress(DownloadId id, FileSize downloaded,
                                                  DownloadStatus status) {
#ifndef NO_SQLITE
    auto* db = static_cast<sqlite3*>(Database::instance().handle());
    if (!db) return Result<void>::failure(Error::make(1, "Database not open"));

    const char* sql = "UPDATE downloads SET downloaded_size=?, status=?, updated_at=datetime('now') WHERE id=?";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<void>::failure(Error::make(rc, sqlite3_errmsg(db)));
    }

    sqlite3_bind_int64(stmt, 1, downloaded);
    sqlite3_bind_int(stmt, 2, static_cast<int>(status));
    sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(id));

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? Result<void>::success()
                               : Result<void>::failure(Error::make(rc, sqlite3_errmsg(db)));
#else
    return Result<void>::success();
#endif
}

Result<void> DownloadRepository::update(const DownloadInfo& info) {
    return save(info);  // INSERT OR REPLACE
}

Result<void> DownloadRepository::remove(DownloadId id) {
#ifndef NO_SQLITE
    auto* db = static_cast<sqlite3*>(Database::instance().handle());
    if (!db) return Result<void>::failure(Error::make(1, "Database not open"));

    std::string sql = "DELETE FROM downloads WHERE id=" + std::to_string(id);
    return Database::instance().execute(sql);
#else
    return Result<void>::success();
#endif
}

std::vector<std::shared_ptr<DownloadInfo>> DownloadRepository::loadAll() {
    std::vector<std::shared_ptr<DownloadInfo>> result;
#ifndef NO_SQLITE
    auto* db = static_cast<sqlite3*>(Database::instance().handle());
    if (!db) return result;

    const char* sql = "SELECT id, url, filename, path, total_size, downloaded_size, "
                      "status, priority, category, etag, last_modified, connection_count, "
                      "retry_count, error_code, error_message FROM downloads ORDER BY id";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return result;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        auto info = std::make_shared<DownloadInfo>();
        info->id = static_cast<DownloadId>(sqlite3_column_int64(stmt, 0));
        info->url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        info->filename = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        info->savePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        info->totalSize = sqlite3_column_int64(stmt, 4);
        info->downloadedSize.store(sqlite3_column_int64(stmt, 5));
        info->status = static_cast<DownloadStatus>(sqlite3_column_int(stmt, 6));
        info->priority = static_cast<DownloadPriority>(sqlite3_column_int(stmt, 7));
        info->category = static_cast<DownloadCategory>(sqlite3_column_int(stmt, 8));
        auto etag = sqlite3_column_text(stmt, 9);
        if (etag) info->etag = reinterpret_cast<const char*>(etag);
        auto lm = sqlite3_column_text(stmt, 10);
        if (lm) info->lastModified = reinterpret_cast<const char*>(lm);
        info->connectionCount = sqlite3_column_int(stmt, 11);
        info->retryCount = sqlite3_column_int(stmt, 12);
        info->lastError.code = sqlite3_column_int(stmt, 13);
        auto errMsg = sqlite3_column_text(stmt, 14);
        if (errMsg) info->lastError.message = reinterpret_cast<const char*>(errMsg);

        result.push_back(info);
    }

    sqlite3_finalize(stmt);
#endif
    return result;
}

std::vector<std::shared_ptr<DownloadInfo>> DownloadRepository::loadIncomplete() {
    std::vector<std::shared_ptr<DownloadInfo>> result;
#ifndef NO_SQLITE
    auto all = loadAll();
    for (auto& info : all) {
        if (info->status == DownloadStatus::DOWNLOADING ||
            info->status == DownloadStatus::CONNECTING ||
            info->status == DownloadStatus::PAUSED ||
            info->status == DownloadStatus::RETRYING) {
            info->status = DownloadStatus::PAUSED;  // Mark as paused for recovery
            result.push_back(info);
        }
    }
#endif
    return result;
}

Result<void> DownloadRepository::saveSegments(DownloadId id, const std::vector<Segment>& segments) {
#ifndef NO_SQLITE
    auto* db = static_cast<sqlite3*>(Database::instance().handle());
    if (!db) return Result<void>::failure(Error::make(1, "Database not open"));

    // Delete old segments
    Database::instance().execute("DELETE FROM download_segments WHERE download_id=" + std::to_string(id));

    const char* sql = "INSERT INTO download_segments (download_id, segment_index, start_offset, "
                      "end_offset, downloaded_bytes, status, retry_count) VALUES (?,?,?,?,?,?,?)";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return Result<void>::failure(Error::make(1, sqlite3_errmsg(db)));
    }

    Database::instance().beginTransaction();

    for (auto& seg : segments) {
        sqlite3_reset(stmt);
        sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(id));
        sqlite3_bind_int(stmt, 2, seg.index);
        sqlite3_bind_int64(stmt, 3, seg.startOffset);
        sqlite3_bind_int64(stmt, 4, seg.endOffset);
        sqlite3_bind_int64(stmt, 5, seg.downloadedBytes);
        sqlite3_bind_int(stmt, 6, static_cast<int>(seg.status));
        sqlite3_bind_int(stmt, 7, seg.retryCount);
        sqlite3_step(stmt);
    }

    sqlite3_finalize(stmt);
    Database::instance().commit();
    return Result<void>::success();
#else
    return Result<void>::success();
#endif
}

std::vector<Segment> DownloadRepository::loadSegments(DownloadId id) {
    std::vector<Segment> result;
#ifndef NO_SQLITE
    auto* db = static_cast<sqlite3*>(Database::instance().handle());
    if (!db) return result;

    std::string sql = "SELECT segment_index, start_offset, end_offset, downloaded_bytes, "
                      "status, retry_count FROM download_segments WHERE download_id=" +
                      std::to_string(id) + " ORDER BY segment_index";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return result;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Segment seg;
        seg.index = sqlite3_column_int(stmt, 0);
        seg.startOffset = sqlite3_column_int64(stmt, 1);
        seg.endOffset = sqlite3_column_int64(stmt, 2);
        seg.downloadedBytes = sqlite3_column_int64(stmt, 3);
        seg.status = static_cast<SegmentStatus>(sqlite3_column_int(stmt, 4));
        seg.retryCount = sqlite3_column_int(stmt, 5);
        result.push_back(seg);
    }

    sqlite3_finalize(stmt);
#endif
    return result;
}

Result<void> DownloadRepository::addToHistory(const DownloadInfo& info) {
#ifndef NO_SQLITE
    auto* db = static_cast<sqlite3*>(Database::instance().handle());
    if (!db) return Result<void>::failure(Error::make(1, "Database not open"));

    const char* sql = "INSERT INTO history (url, filename, path, file_size, duration_sec, "
                      "avg_speed, status, completed_at) VALUES (?,?,?,?,?,?,?,datetime('now'))";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return Result<void>::failure(Error::make(1, sqlite3_errmsg(db)));
    }

    sqlite3_bind_text(stmt, 1, info.url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, info.filename.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, info.savePath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, info.totalSize);

    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        info.completedAt - info.createdAt);
    sqlite3_bind_int64(stmt, 5, elapsed.count());
    sqlite3_bind_int64(stmt, 6, info.speedTracker.averageSpeed());
    sqlite3_bind_text(stmt, 7, downloadStatusToString(info.status), -1, SQLITE_STATIC);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return Result<void>::success();
#else
    return Result<void>::success();
#endif
}

std::vector<DownloadRepository::HistoryEntry> DownloadRepository::loadHistory(int limit) {
    std::vector<HistoryEntry> result;
#ifndef NO_SQLITE
    auto* db = static_cast<sqlite3*>(Database::instance().handle());
    if (!db) return result;

    std::string sql = "SELECT url, filename, path, file_size, duration_sec, avg_speed, "
                      "status, completed_at FROM history ORDER BY id DESC LIMIT " +
                      std::to_string(limit);

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return result;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        HistoryEntry entry;
        auto col = [stmt](int i) -> std::string {
            auto* t = sqlite3_column_text(stmt, i);
            return t ? reinterpret_cast<const char*>(t) : "";
        };
        entry.url = col(0);
        entry.filename = col(1);
        entry.path = col(2);
        entry.fileSize = sqlite3_column_int64(stmt, 3);
        entry.durationSec = sqlite3_column_int(stmt, 4);
        entry.avgSpeed = sqlite3_column_int64(stmt, 5);
        entry.status = col(6);
        entry.completedAt = col(7);
        result.push_back(entry);
    }

    sqlite3_finalize(stmt);
#endif
    return result;
}

Result<void> DownloadRepository::clearHistory() {
    return Database::instance().execute("DELETE FROM history");
}

} // namespace ps5dm
