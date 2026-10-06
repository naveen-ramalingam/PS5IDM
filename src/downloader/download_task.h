#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Download Task
// ═══════════════════════════════════════════════════════════════════════════════
#include "common/types.h"
#include "downloader/segment.h"
#include "downloader/download_worker.h"
#include "downloader/retry_manager.h"
#include "network/http_headers.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

namespace ps5dm {

/// Represents a complete download with all its state
struct DownloadInfo {
    DownloadId id = 0;
    std::string url;
    std::string filename;
    std::string savePath;        // directory
    std::string filePath;        // full path = savePath/filename
    std::string tempFilePath;    // temp file during download

    FileSize totalSize = -1;     // -1 = unknown
    std::atomic<FileSize> downloadedSize{0};
    DownloadStatus status = DownloadStatus::PENDING;
    DownloadPriority priority = DownloadPriority::NORMAL;
    DownloadCategory category = DownloadCategory::OTHER;

    std::string etag;
    std::string lastModified;
    std::string contentType;
    bool supportsRanges = false;

    int connectionCount = 8;
    int retryCount = 0;
    int maxRetries = 5;
    Error lastError;

    ChecksumAlgorithm checksumAlgorithm = ChecksumAlgorithm::NONE;
    std::string expectedChecksum;
    std::string calculatedChecksum;

    Timestamp createdAt;
    Timestamp updatedAt;
    Timestamp completedAt;

    SpeedTracker speedTracker;
};

/// A download task that manages workers and segments for one URL
class DownloadTask {
public:
    explicit DownloadTask(std::shared_ptr<DownloadInfo> info);
    ~DownloadTask();

    /// Start the download (probing + segmented download)
    void start();

    /// Pause the download
    void pause();

    /// Resume a paused download
    void resume();

    /// Cancel the download
    void cancel();

    /// Retry a failed download
    void retry();

    /// Get download info
    std::shared_ptr<DownloadInfo> info() const { return info_; }

    /// Get download ID
    DownloadId id() const { return info_->id; }

    /// Get current status
    DownloadStatus status() const { return info_->status; }

    /// Get segment scheduler
    const SegmentScheduler& scheduler() const { return scheduler_; }

    /// Is the download active (workers running)?
    bool isActive() const;

private:
    void probeServer();
    void startWorkers();
    void stopWorkers();
    void onSegmentProgress(int segmentIndex, FileSize bytesWritten);
    void onSegmentComplete(int segmentIndex, bool success, Error error);
    void checkCompletion();
    void preallocateFile();
    void finalize();
    void publishProgress();
    void publishStatus(DownloadStatus status);

    std::shared_ptr<DownloadInfo> info_;
    SegmentScheduler scheduler_;
    RetryManager retryManager_;
    std::vector<std::unique_ptr<DownloadWorker>> workers_;
    std::mutex mutex_;
    std::thread controlThread_;
    std::atomic<bool> stopping_{false};
};

} // namespace ps5dm
