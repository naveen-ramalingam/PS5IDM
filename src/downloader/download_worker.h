#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Download Worker
// ═══════════════════════════════════════════════════════════════════════════════
#include "common/types.h"
#include "downloader/segment.h"
#include "network/http_client.h"
#include <thread>
#include <atomic>
#include <functional>

namespace ps5dm {

/// A single download worker thread that downloads one segment at a time
class DownloadWorker {
public:
    using ProgressCallback = std::function<void(int segmentIndex, FileSize bytesWritten)>;
    using CompletionCallback = std::function<void(int segmentIndex, bool success, Error error)>;

    explicit DownloadWorker(int workerId);
    ~DownloadWorker();

    /// Start the worker thread
    void start();

    /// Stop the worker gracefully
    void stop();

    /// Assign a segment to download
    void assignSegment(Segment* segment, const std::string& url,
                       const std::string& filePath,
                       const HttpHeaders& extraHeaders = {});

    /// Check if worker is busy
    bool isBusy() const { return busy_.load(); }

    /// Check if worker is running
    bool isRunning() const { return running_.load(); }

    /// Pause/unpause
    void pause();
    void resume();
    bool isPaused() const { return paused_.load(); }

    /// Set callbacks
    void setProgressCallback(ProgressCallback cb) { progressCallback_ = std::move(cb); }
    void setCompletionCallback(CompletionCallback cb) { completionCallback_ = std::move(cb); }

    /// Worker ID
    int id() const { return workerId_; }

    /// Current segment
    const Segment* currentSegment() const { return currentSegment_; }

private:
    void workerLoop();
    void downloadSegment();
    bool writeToFile(const std::string& filePath, FileOffset offset,
                     const void* data, size_t size);

    int workerId_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> busy_{false};
    std::atomic<bool> paused_{false};
    std::atomic<bool> stopRequested_{false};

    // Current assignment
    Segment* currentSegment_ = nullptr;
    std::string currentUrl_;
    std::string currentFilePath_;
    HttpHeaders currentHeaders_;
    std::mutex assignMutex_;
    std::condition_variable assignCv_;
    bool hasAssignment_ = false;

    ProgressCallback progressCallback_;
    CompletionCallback completionCallback_;
};

} // namespace ps5dm
