#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Download Repository
// ═══════════════════════════════════════════════════════════════════════════════
#include "common/types.h"
#include "downloader/download_task.h"
#include <vector>
#include <memory>

namespace ps5dm {

/// Data access layer for download persistence
class DownloadRepository {
public:
    static DownloadRepository& instance();

    /// Save a download record
    Result<void> save(const DownloadInfo& info);

    /// Update download progress
    Result<void> updateProgress(DownloadId id, FileSize downloaded, DownloadStatus status);

    /// Update full download state
    Result<void> update(const DownloadInfo& info);

    /// Delete a download record
    Result<void> remove(DownloadId id);

    /// Load all downloads
    std::vector<std::shared_ptr<DownloadInfo>> loadAll();

    /// Load incomplete downloads for recovery
    std::vector<std::shared_ptr<DownloadInfo>> loadIncomplete();

    /// Save segments for a download
    Result<void> saveSegments(DownloadId id, const std::vector<Segment>& segments);

    /// Load segments for a download
    std::vector<Segment> loadSegments(DownloadId id);

    /// Add to history
    Result<void> addToHistory(const DownloadInfo& info);

    /// Load history
    struct HistoryEntry {
        std::string filename;
        std::string url;
        std::string path;
        FileSize fileSize;
        int durationSec;
        FileSize avgSpeed;
        std::string status;
        std::string completedAt;
    };
    std::vector<HistoryEntry> loadHistory(int limit = 100);

    /// Clear history
    Result<void> clearHistory();

    DownloadRepository(const DownloadRepository&) = delete;
    DownloadRepository& operator=(const DownloadRepository&) = delete;

private:
    DownloadRepository() = default;
};

} // namespace ps5dm
