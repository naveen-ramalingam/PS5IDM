// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Download Task Implementation
// ═══════════════════════════════════════════════════════════════════════════════
#include "downloader/download_task.h"
#include "downloader/connection_manager.h"
#include "core/logger.h"
#include "core/event_bus.h"
#include "settings/settings.h"
#include "platform/paths.h"

#include <algorithm>
#include <cstring>
#include <sys/stat.h>

namespace ps5dm {

static const char* TAG = "DownloadTask";

DownloadTask::DownloadTask(std::shared_ptr<DownloadInfo> info)
    : info_(std::move(info))
    , retryManager_(info_->maxRetries, Settings::instance().retryDelayBaseSec()) {}

DownloadTask::~DownloadTask() {
    cancel();
}

void DownloadTask::start() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (info_->status == DownloadStatus::DOWNLOADING ||
        info_->status == DownloadStatus::CONNECTING) {
        return;  // Already running
    }

    stopping_ = false;
    info_->status = DownloadStatus::CONNECTING;
    info_->createdAt = std::chrono::system_clock::now();
    publishStatus(DownloadStatus::CONNECTING);

    // Run probe + download on control thread
    controlThread_ = std::thread([this] {
        probeServer();
        if (!stopping_.load() && info_->status != DownloadStatus::FAILED) {
            startWorkers();
        }
    });
    controlThread_.detach();
}

void DownloadTask::probeServer() {
    LOG_INFO(TAG, "Probing server for: " + info_->url);

    HttpClient client;
    auto& settings = Settings::instance();
    client.setUserAgent(settings.userAgent());
    client.setConnectTimeout(settings.connectTimeoutSec());

    // HEAD request to determine file properties
    auto headResult = client.head(info_->url);
    if (!headResult.ok()) {
        // HEAD might not be supported; try GET with range 0-0
        LOG_WARN(TAG, "HEAD failed, trying GET probe: " + headResult.error.message);

        HttpRequest probeReq;
        probeReq.url = info_->url;
        probeReq.rangeStart = 0;
        probeReq.rangeEnd = 0;
        probeReq.connectTimeoutSec = settings.connectTimeoutSec();
        probeReq.transferTimeoutSec = 10;

        auto getResult = client.download(probeReq);
        if (!getResult.ok()) {
            info_->lastError = headResult.error;
            info_->status = DownloadStatus::FAILED;
            publishStatus(DownloadStatus::FAILED);
            return;
        }

        auto& resp = getResult.get();
        info_->totalSize = resp.headers.getContentLength();
        info_->supportsRanges = (resp.statusCode == 206);
        info_->etag = resp.headers.getETag();
        info_->lastModified = resp.headers.getLastModified();
        info_->contentType = resp.headers.getContentType();

        // Try to get filename from Content-Disposition
        auto cdFilename = resp.headers.getContentDispositionFilename();
        if (!cdFilename.empty() && info_->filename.empty()) {
            info_->filename = cdFilename;
        }

        if (resp.effectiveUrl != info_->url) {
            LOG_INFO(TAG, "Redirected to: " + resp.effectiveUrl);
            info_->url = resp.effectiveUrl;
        }
    } else {
        auto& resp = headResult.get();

        info_->totalSize = resp.contentLength;
        info_->supportsRanges = resp.supportsRanges;
        info_->etag = resp.headers.getETag();
        info_->lastModified = resp.headers.getLastModified();
        info_->contentType = resp.headers.getContentType();

        auto cdFilename = resp.headers.getContentDispositionFilename();
        if (!cdFilename.empty() && info_->filename.empty()) {
            info_->filename = cdFilename;
        }

        if (resp.effectiveUrl != info_->url) {
            LOG_INFO(TAG, "Redirected to: " + resp.effectiveUrl);
            info_->url = resp.effectiveUrl;
        }

        // Check status code
        if (resp.statusCode >= 400) {
            info_->lastError = Error::http(resp.statusCode,
                "Server returned HTTP " + std::to_string(resp.statusCode));
            info_->status = DownloadStatus::FAILED;
            publishStatus(DownloadStatus::FAILED);
            return;
        }
    }

    // Derive filename from URL if not set
    if (info_->filename.empty()) {
        info_->filename = HttpClient::filenameFromUrl(info_->url);
    }

    // Set file paths
    info_->filePath = Paths::join(info_->savePath, info_->filename);
    info_->tempFilePath = info_->filePath + ".ps5dm.tmp";

    // Determine category
    info_->category = categorizeByExtension(info_->filename);

    LOG_INFO(TAG, "Probe complete: " + info_->filename +
             " size=" + formatSize(info_->totalSize) +
             " ranges=" + (info_->supportsRanges ? "yes" : "no"));
}

void DownloadTask::startWorkers() {
    auto& settings = Settings::instance();

    // Determine connection count
    int conns = info_->connectionCount;
    if (!info_->supportsRanges || info_->totalSize <= 0) {
        conns = 1;  // Single connection if no range support
        LOG_INFO(TAG, "Range requests not supported, using single connection");
    }

    // Create segments
    if (info_->totalSize > 0) {
        scheduler_.createSegments(info_->totalSize, settings.segmentSize());
    } else {
        // Unknown size - single segment
        scheduler_.createSegments(0, settings.segmentSize());
        conns = 1;
    }

    // Preallocate file
    if (info_->totalSize > 0) {
        preallocateFile();
    }

    info_->status = DownloadStatus::DOWNLOADING;
    publishStatus(DownloadStatus::DOWNLOADING);
    info_->speedTracker.reset();

    LOG_INFO(TAG, "Starting " + std::to_string(conns) + " workers for " + info_->filename);

    // Create workers
    workers_.clear();
    for (int i = 0; i < conns; ++i) {
        auto worker = std::make_unique<DownloadWorker>(i);

        worker->setProgressCallback([this](int segIdx, FileSize bytes) {
            onSegmentProgress(segIdx, bytes);
        });

        worker->setCompletionCallback([this](int segIdx, bool ok, Error err) {
            onSegmentComplete(segIdx, ok, std::move(err));
        });

        worker->start();

        // Assign first available segment
        auto* seg = scheduler_.getNextSegment();
        if (seg) {
            worker->assignSegment(seg, info_->url, info_->tempFilePath);
        }

        workers_.push_back(std::move(worker));
    }
}

void DownloadTask::preallocateFile() {
    if (info_->totalSize <= 0) return;

    LOG_DEBUG(TAG, "Preallocating " + formatSize(info_->totalSize) + " for " + info_->filename);

    FILE* fp = fopen(info_->tempFilePath.c_str(), "w+b");
    if (!fp) {
        LOG_ERROR(TAG, "Failed to create temp file: " + info_->tempFilePath);
        return;
    }

    // Seek to end and write one byte to allocate space
    if (fseeko(fp, info_->totalSize - 1, SEEK_SET) == 0) {
        char zero = 0;
        fwrite(&zero, 1, 1, fp);
    }

    fclose(fp);
}

void DownloadTask::onSegmentProgress(int /*segmentIndex*/, FileSize bytesWritten) {
    info_->downloadedSize.fetch_add(bytesWritten);
    info_->speedTracker.addSample(bytesWritten);
    info_->updatedAt = std::chrono::system_clock::now();

    publishProgress();
}

void DownloadTask::onSegmentComplete(int segmentIndex, bool success, Error error) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (stopping_.load()) return;

    if (success) {
        scheduler_.completeSegment(segmentIndex);
        LOG_DEBUG(TAG, "Segment " + std::to_string(segmentIndex) + " completed");

        // Try to assign next segment to the worker that just finished
        auto* nextSeg = scheduler_.getNextSegment();
        if (nextSeg) {
            // Find the worker that completed
            for (auto& w : workers_) {
                if (!w->isBusy()) {
                    w->assignSegment(nextSeg, info_->url, info_->tempFilePath);
                    break;
                }
            }
        } else {
            // No more pending segments - try dynamic splitting
            auto* splitSeg = scheduler_.splitLargestRunning(
                Settings::instance().segmentSize() / 2);
            if (splitSeg) {
                for (auto& w : workers_) {
                    if (!w->isBusy()) {
                        w->assignSegment(splitSeg, info_->url, info_->tempFilePath);
                        break;
                    }
                }
            }
        }

        checkCompletion();
    } else {
        scheduler_.failSegment(segmentIndex);
        info_->lastError = error;

        if (retryManager_.shouldRetry(info_->retryCount, error.httpStatus) &&
            RetryManager::isRetryableError(error)) {
            info_->retryCount++;
            auto delay = retryManager_.getDelay(info_->retryCount);

            LOG_WARN(TAG, "Segment " + std::to_string(segmentIndex) +
                     " failed, retrying in " + std::to_string(delay.count()) + "s: " + error.message);

            info_->status = DownloadStatus::RETRYING;

            // Schedule retry
            std::thread([this, segmentIndex, delay] {
                std::this_thread::sleep_for(delay);
                if (!stopping_.load()) {
                    scheduler_.retrySegment(segmentIndex);
                    auto* seg = scheduler_.getNextSegment();
                    if (seg) {
                        for (auto& w : workers_) {
                            if (!w->isBusy()) {
                                w->assignSegment(seg, info_->url, info_->tempFilePath);
                                break;
                            }
                        }
                    }
                    if (info_->status == DownloadStatus::RETRYING) {
                        info_->status = DownloadStatus::DOWNLOADING;
                    }
                }
            }).detach();
        } else {
            LOG_ERROR(TAG, "Segment " + std::to_string(segmentIndex) +
                      " permanently failed: " + error.message);
            info_->status = DownloadStatus::FAILED;
            publishStatus(DownloadStatus::FAILED);
        }
    }
}

void DownloadTask::checkCompletion() {
    if (scheduler_.allComplete()) {
        LOG_INFO(TAG, "All segments complete for " + info_->filename);
        finalize();
    }
}

void DownloadTask::finalize() {
    info_->status = DownloadStatus::MERGING;
    publishStatus(DownloadStatus::MERGING);

    // Rename temp file to final
    if (rename(info_->tempFilePath.c_str(), info_->filePath.c_str()) != 0) {
        // If rename fails (cross-device), try copy
        LOG_WARN(TAG, "Rename failed, file may already be at: " + info_->filePath);
    }

    info_->completedAt = std::chrono::system_clock::now();
    info_->status = DownloadStatus::COMPLETED;
    info_->downloadedSize.store(info_->totalSize);

    LOG_INFO(TAG, "Download complete: " + info_->filename + " (" +
             formatSize(info_->totalSize) + ")");

    publishStatus(DownloadStatus::COMPLETED);

    EventBus::instance().queueEvent(Event(EventType::UI_NOTIFICATION,
        NotificationEvent{"Download Complete", info_->filename + " finished", LogLevel::INFO}));
}

void DownloadTask::pause() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (info_->status != DownloadStatus::DOWNLOADING) return;

    for (auto& w : workers_) {
        w->pause();
    }
    info_->status = DownloadStatus::PAUSED;
    publishStatus(DownloadStatus::PAUSED);
    LOG_INFO(TAG, "Paused: " + info_->filename);
}

void DownloadTask::resume() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (info_->status != DownloadStatus::PAUSED) return;

    for (auto& w : workers_) {
        w->resume();
    }
    info_->status = DownloadStatus::DOWNLOADING;
    publishStatus(DownloadStatus::DOWNLOADING);
    LOG_INFO(TAG, "Resumed: " + info_->filename);
}

void DownloadTask::cancel() {
    stopping_ = true;
    stopWorkers();

    info_->status = DownloadStatus::CANCELLED;
    publishStatus(DownloadStatus::CANCELLED);
    LOG_INFO(TAG, "Cancelled: " + info_->filename);
}

void DownloadTask::retry() {
    info_->retryCount = 0;
    info_->lastError = Error::none();
    scheduler_.resetAll();
    info_->downloadedSize = 0;
    start();
}

void DownloadTask::stopWorkers() {
    for (auto& w : workers_) {
        w->stop();
    }
    workers_.clear();

    if (controlThread_.joinable()) {
        controlThread_.join();
    }
}

bool DownloadTask::isActive() const {
    auto s = info_->status;
    return s == DownloadStatus::CONNECTING ||
           s == DownloadStatus::DOWNLOADING ||
           s == DownloadStatus::RETRYING ||
           s == DownloadStatus::MERGING;
}

void DownloadTask::publishProgress() {
    auto totalSize = info_->totalSize;
    auto downloaded = info_->downloadedSize.load();
    float pct = totalSize > 0 ? (static_cast<float>(downloaded) / static_cast<float>(totalSize)) * 100.0f : 0.0f;

    DownloadProgressEvent evt;
    evt.downloadId = info_->id;
    evt.downloadedBytes = downloaded;
    evt.totalBytes = totalSize;
    evt.speed = info_->speedTracker.currentSpeed();
    evt.percentage = pct;

    EventBus::instance().queueEvent(Event(EventType::DOWNLOAD_PROGRESS, evt));
}

void DownloadTask::publishStatus(DownloadStatus status) {
    DownloadStatusEvent evt;
    evt.downloadId = info_->id;
    evt.status = status;
    evt.filename = info_->filename;
    evt.error = info_->lastError;

    EventType type;
    switch (status) {
        case DownloadStatus::CONNECTING:  type = EventType::DOWNLOAD_STARTED; break;
        case DownloadStatus::DOWNLOADING: type = EventType::DOWNLOAD_STARTED; break;
        case DownloadStatus::PAUSED:      type = EventType::DOWNLOAD_PAUSED; break;
        case DownloadStatus::COMPLETED:   type = EventType::DOWNLOAD_COMPLETED; break;
        case DownloadStatus::FAILED:      type = EventType::DOWNLOAD_FAILED; break;
        case DownloadStatus::CANCELLED:   type = EventType::DOWNLOAD_CANCELLED; break;
        default:                          type = EventType::DOWNLOAD_STARTED; break;
    }

    EventBus::instance().queueEvent(Event(type, evt));
}

} // namespace ps5dm
