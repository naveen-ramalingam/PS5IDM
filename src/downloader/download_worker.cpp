// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Download Worker Implementation
// ═══════════════════════════════════════════════════════════════════════════════
#include "downloader/download_worker.h"
#include "downloader/connection_manager.h"
#include "core/logger.h"
#include "settings/settings.h"

#include <fstream>
#include <cstring>

namespace ps5dm {

static const char* TAG = "Worker";

DownloadWorker::DownloadWorker(int workerId) : workerId_(workerId) {}

DownloadWorker::~DownloadWorker() {
    stop();
}

void DownloadWorker::start() {
    if (running_.load()) return;
    running_ = true;
    stopRequested_ = false;
    thread_ = std::thread(&DownloadWorker::workerLoop, this);
}

void DownloadWorker::stop() {
    stopRequested_ = true;
    paused_ = false;

    {
        std::lock_guard<std::mutex> lock(assignMutex_);
        hasAssignment_ = true;  // Wake up waiting thread
    }
    assignCv_.notify_all();

    if (thread_.joinable()) {
        thread_.join();
    }
    running_ = false;
}

void DownloadWorker::pause() {
    paused_ = true;
}

void DownloadWorker::resume() {
    paused_ = false;
}

void DownloadWorker::assignSegment(Segment* segment, const std::string& url,
                                    const std::string& filePath,
                                    const HttpHeaders& extraHeaders) {
    std::lock_guard<std::mutex> lock(assignMutex_);
    currentSegment_ = segment;
    currentUrl_ = url;
    currentFilePath_ = filePath;
    currentHeaders_ = extraHeaders;
    hasAssignment_ = true;
    assignCv_.notify_one();
}

void DownloadWorker::workerLoop() {
    std::string tag = TAG + std::string("[") + std::to_string(workerId_) + "]";

    LOG_DEBUG(tag, "Worker started");

    while (!stopRequested_.load()) {
        // Wait for assignment
        {
            std::unique_lock<std::mutex> lock(assignMutex_);
            assignCv_.wait(lock, [this] { return hasAssignment_ || stopRequested_.load(); });

            if (stopRequested_.load()) break;
            if (!hasAssignment_) continue;
            hasAssignment_ = false;
        }

        if (!currentSegment_ || stopRequested_.load()) continue;

        busy_ = true;
        downloadSegment();
        busy_ = false;
        currentSegment_ = nullptr;
    }

    LOG_DEBUG(tag, "Worker stopped");
}

void DownloadWorker::downloadSegment() {
    std::string tag = TAG + std::string("[") + std::to_string(workerId_) + "]";

    Segment* seg = currentSegment_;
    if (!seg) return;

    LOG_DEBUG(tag, "Downloading segment " + std::to_string(seg->index) +
              " [" + std::to_string(seg->startOffset + seg->downloadedBytes) +
              "-" + std::to_string(seg->endOffset) + "]");

    ConnectionManager::instance().addConnection();

    HttpClient client;
    auto& settings = Settings::instance();

    client.setUserAgent(settings.userAgent());
    client.setConnectTimeout(settings.connectTimeoutSec());

    // Don't set overall transfer timeout for large segments - use low-speed detection instead
    // client.setTransferTimeout() is handled by curl's low-speed settings

    HttpRequest request;
    request.url = currentUrl_;
    request.headers = currentHeaders_;
    request.connectTimeoutSec = settings.connectTimeoutSec();
    request.transferTimeoutSec = 0;  // No hard timeout; rely on low-speed abort
    request.followRedirects = true;

    // Set range
    FileOffset resumeFrom = seg->startOffset + seg->downloadedBytes;
    request.rangeStart = resumeFrom;
    if (seg->endOffset >= 0) {
        request.rangeEnd = seg->endOffset;
    }

    // Speed limit per connection
    auto perConnLimit = ConnectionManager::instance().perConnectionLimit();
    if (perConnLimit > 0) {
        client.setSpeedLimit(perConnLimit);
    }

    // Write callback - writes directly to file at correct offset
    FileOffset writeOffset = resumeFrom;
    std::atomic<bool>& pauseFlag = paused_;
    std::atomic<bool>& stopFlag = stopRequested_;

    request.writeCallback = [this, seg, &writeOffset, &pauseFlag, &stopFlag, &tag]
                            (const void* data, size_t size) -> size_t {
        // Check for pause
        while (pauseFlag.load() && !stopFlag.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (stopFlag.load()) return 0;  // Abort transfer

        // Write to file
        if (!writeToFile(currentFilePath_, writeOffset, data, size)) {
            LOG_ERROR(tag, "Failed to write to file at offset " + std::to_string(writeOffset));
            return 0;  // Abort
        }

        writeOffset += static_cast<FileOffset>(size);
        seg->downloadedBytes += static_cast<FileSize>(size);

        // Report progress
        ConnectionManager::instance().addBytesTransferred(static_cast<FileSize>(size));
        if (progressCallback_) {
            progressCallback_(seg->index, static_cast<FileSize>(size));
        }

        return size;
    };

    // Progress callback (for curl-level progress)
    request.progressCallback = [&stopFlag, &pauseFlag](int64_t /*downloaded*/, int64_t /*total*/) -> bool {
        return !stopFlag.load();
    };

    // Execute download
    auto result = client.download(request);

    ConnectionManager::instance().removeConnection();

    if (stopRequested_.load()) {
        LOG_DEBUG(tag, "Segment " + std::to_string(seg->index) + " stopped by request");
        return;
    }

    if (result.ok()) {
        auto& resp = result.get();
        if (resp.statusCode == 200 || resp.statusCode == 206) {
            LOG_INFO(tag, "Segment " + std::to_string(seg->index) + " completed: " +
                     formatSize(seg->downloadedBytes));
            if (completionCallback_) {
                completionCallback_(seg->index, true, Error::none());
            }
        } else {
            Error err = Error::http(resp.statusCode,
                "HTTP " + std::to_string(resp.statusCode));
            LOG_WARN(tag, "Segment " + std::to_string(seg->index) +
                     " HTTP error: " + std::to_string(resp.statusCode));
            if (completionCallback_) {
                completionCallback_(seg->index, false, err);
            }
        }
    } else {
        LOG_WARN(tag, "Segment " + std::to_string(seg->index) +
                 " failed: " + result.error.message);
        if (completionCallback_) {
            completionCallback_(seg->index, false, result.error);
        }
    }
}

bool DownloadWorker::writeToFile(const std::string& filePath, FileOffset offset,
                                  const void* data, size_t size) {
    // Use POSIX pwrite for random-access write without seeking
    FILE* fp = fopen(filePath.c_str(), "r+b");
    if (!fp) {
        // Try creating the file
        fp = fopen(filePath.c_str(), "w+b");
        if (!fp) return false;
    }

    if (fseeko(fp, offset, SEEK_SET) != 0) {
        fclose(fp);
        return false;
    }

    size_t written = fwrite(data, 1, size, fp);
    fclose(fp);

    return written == size;
}

} // namespace ps5dm
