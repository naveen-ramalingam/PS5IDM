#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Download Manager
// ═══════════════════════════════════════════════════════════════════════════════
#include "common/types.h"
#include "downloader/download_task.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <functional>

namespace ps5dm {

/// Parameters for adding a new download
struct AddDownloadParams {
    std::string url;
    std::string filename;     // empty = auto-detect
    std::string savePath;     // empty = default download dir
    int connections = 0;      // 0 = use setting
    DownloadPriority priority = DownloadPriority::NORMAL;
    bool startImmediately = true;
    ChecksumAlgorithm checksumAlgorithm = ChecksumAlgorithm::NONE;
    std::string expectedChecksum;
};

/// Filter for listing downloads
struct DownloadFilter {
    DownloadStatus statusFilter = static_cast<DownloadStatus>(-1);  // -1 = all
    DownloadCategory categoryFilter = static_cast<DownloadCategory>(-1);
    std::string searchText;
};

/// Central download management - queue, scheduling, persistence
class DownloadManager {
public:
    static DownloadManager& instance();

    /// Initialize the manager
    Result<void> init();

    /// Shutdown cleanly
    void shutdown();

    // ─── Download operations ────────────────────────────────────────────

    /// Add a new download
    Result<DownloadId> addDownload(const AddDownloadParams& params);

    /// Start a download
    Result<void> startDownload(DownloadId id);

    /// Pause a download
    Result<void> pauseDownload(DownloadId id);

    /// Resume a download
    Result<void> resumeDownload(DownloadId id);

    /// Cancel a download
    Result<void> cancelDownload(DownloadId id);

    /// Retry a failed download
    Result<void> retryDownload(DownloadId id);

    /// Remove a download from the list
    Result<void> removeDownload(DownloadId id, bool deleteFile = false);

    /// Pause all active downloads
    void pauseAll();

    /// Resume all paused downloads
    void resumeAll();

    // ─── Queue management ───────────────────────────────────────────────

    /// Move download up in queue
    void moveUp(DownloadId id);

    /// Move download down in queue
    void moveDown(DownloadId id);

    /// Process the queue (start next queued downloads if slots available)
    void processQueue();

    // ─── Query ──────────────────────────────────────────────────────────

    /// Get download info
    std::shared_ptr<DownloadInfo> getDownload(DownloadId id) const;

    /// Get all downloads (optionally filtered)
    std::vector<std::shared_ptr<DownloadInfo>> getDownloads(
        const DownloadFilter& filter = {}) const;

    /// Get active download count
    int activeCount() const;

    /// Get queued download count
    int queuedCount() const;

    /// Check for duplicate URL
    bool hasDuplicateUrl(const std::string& url) const;

    // ─── Recovery ───────────────────────────────────────────────────────

    /// Recover incomplete downloads from previous session
    int recoverDownloads();

    DownloadManager(const DownloadManager&) = delete;
    DownloadManager& operator=(const DownloadManager&) = delete;

private:
    DownloadManager();

    DownloadId nextId();
    DownloadTask* findTask(DownloadId id);

    mutable std::mutex mutex_;
    std::vector<DownloadId> queue_;  // ordered queue
    std::unordered_map<DownloadId, std::shared_ptr<DownloadTask>> tasks_;
    std::unordered_map<DownloadId, std::shared_ptr<DownloadInfo>> downloads_;
    std::atomic<DownloadId> nextId_{1};
    bool initialized_ = false;
};

} // namespace ps5dm
