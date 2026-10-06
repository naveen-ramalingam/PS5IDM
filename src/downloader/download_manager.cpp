// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Download Manager Implementation
// ═══════════════════════════════════════════════════════════════════════════════
#include "downloader/download_manager.h"
#include "core/logger.h"
#include "core/event_bus.h"
#include "settings/settings.h"
#include "platform/paths.h"

#include <algorithm>
#include <sys/stat.h>

namespace ps5dm {

static const char* TAG = "DownloadMgr";

DownloadManager& DownloadManager::instance() {
    static DownloadManager mgr;
    return mgr;
}

DownloadManager::DownloadManager() = default;

Result<void> DownloadManager::init() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_) return Result<void>::success();

    // Ensure download directory exists
    auto& settings = Settings::instance();
    auto downloadDir = settings.downloadDirectory();
    auto tempDir = settings.tempDirectory();

    mkdir(downloadDir.c_str(), 0755);
    mkdir(tempDir.c_str(), 0755);

    initialized_ = true;
    LOG_INFO(TAG, "Download manager initialized");
    LOG_INFO(TAG, "Download dir: " + downloadDir);
    LOG_INFO(TAG, "Temp dir: " + tempDir);

    return Result<void>::success();
}

void DownloadManager::shutdown() {
    LOG_INFO(TAG, "Shutting down download manager...");

    pauseAll();

    std::lock_guard<std::mutex> lock(mutex_);
    tasks_.clear();
    initialized_ = false;

    LOG_INFO(TAG, "Download manager shut down");
}

DownloadId DownloadManager::nextId() {
    return nextId_++;
}

Result<DownloadId> DownloadManager::addDownload(const AddDownloadParams& params) {
    // Validate URL
    if (!HttpClient::isValidUrl(params.url)) {
        return Result<DownloadId>::failure(
            Error::make(1, "Invalid URL. Must start with http:// or https://"));
    }

    auto& settings = Settings::instance();
    std::lock_guard<std::mutex> lock(mutex_);

    auto info = std::make_shared<DownloadInfo>();
    info->id = nextId();
    info->url = params.url;
    info->filename = params.filename;
    info->savePath = params.savePath.empty() ? settings.downloadDirectory() : params.savePath;
    info->connectionCount = params.connections > 0 ? params.connections : settings.connectionsPerDownload();
    info->priority = params.priority;
    info->maxRetries = settings.maxRetries();
    info->checksumAlgorithm = params.checksumAlgorithm;
    info->expectedChecksum = params.expectedChecksum;
    info->createdAt = std::chrono::system_clock::now();
    info->updatedAt = info->createdAt;

    // Derive filename from URL if not provided
    if (info->filename.empty()) {
        info->filename = HttpClient::filenameFromUrl(info->url);
    }

    downloads_[info->id] = info;

    auto task = std::make_shared<DownloadTask>(info);
    tasks_[info->id] = task;

    if (params.startImmediately) {
        // Check if we can start immediately
        if (activeCount() < settings.maxActiveDownloads()) {
            info->status = DownloadStatus::PENDING;
            queue_.push_back(info->id);
        } else {
            info->status = DownloadStatus::QUEUED;
            queue_.push_back(info->id);
        }
    } else {
        info->status = DownloadStatus::QUEUED;
        queue_.push_back(info->id);
    }

    LOG_INFO(TAG, "Added download #" + std::to_string(info->id) + ": " + info->url);

    EventBus::instance().queueEvent(Event(EventType::DOWNLOAD_ADDED,
        DownloadStatusEvent{info->id, info->status, info->filename, Error::none()}));

    // Process queue to start if possible
    // (unlock first since processQueue locks)
    // We'll call it after returning
    return Result<DownloadId>::success(info->id);
}

Result<void> DownloadManager::startDownload(DownloadId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto task = findTask(id);
    if (!task) {
        return Result<void>::failure(Error::make(1, "Download not found"));
    }
    task->start();
    return Result<void>::success();
}

Result<void> DownloadManager::pauseDownload(DownloadId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto task = findTask(id);
    if (!task) {
        return Result<void>::failure(Error::make(1, "Download not found"));
    }
    task->pause();
    return Result<void>::success();
}

Result<void> DownloadManager::resumeDownload(DownloadId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto task = findTask(id);
    if (!task) {
        return Result<void>::failure(Error::make(1, "Download not found"));
    }
    task->resume();
    return Result<void>::success();
}

Result<void> DownloadManager::cancelDownload(DownloadId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto task = findTask(id);
    if (!task) {
        return Result<void>::failure(Error::make(1, "Download not found"));
    }
    task->cancel();
    return Result<void>::success();
}

Result<void> DownloadManager::retryDownload(DownloadId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto task = findTask(id);
    if (!task) {
        return Result<void>::failure(Error::make(1, "Download not found"));
    }
    task->retry();
    return Result<void>::success();
}

Result<void> DownloadManager::removeDownload(DownloadId id, bool deleteFile) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto taskIt = tasks_.find(id);
    if (taskIt != tasks_.end()) {
        taskIt->second->cancel();
        tasks_.erase(taskIt);
    }

    auto dlIt = downloads_.find(id);
    if (dlIt != downloads_.end()) {
        if (deleteFile) {
            auto& info = dlIt->second;
            if (!info->filePath.empty()) {
                remove(info->filePath.c_str());
            }
            if (!info->tempFilePath.empty()) {
                remove(info->tempFilePath.c_str());
            }
        }
        downloads_.erase(dlIt);
    }

    queue_.erase(std::remove(queue_.begin(), queue_.end(), id), queue_.end());

    EventBus::instance().queueEvent(Event(EventType::DOWNLOAD_REMOVED,
        DownloadStatusEvent{id, DownloadStatus::CANCELLED, "", Error::none()}));

    LOG_INFO(TAG, "Removed download #" + std::to_string(id));
    return Result<void>::success();
}

void DownloadManager::pauseAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [id, task] : tasks_) {
        if (task->isActive()) {
            task->pause();
        }
    }
    LOG_INFO(TAG, "Paused all downloads");
}

void DownloadManager::resumeAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [id, task] : tasks_) {
        if (task->status() == DownloadStatus::PAUSED) {
            task->resume();
        }
    }
    LOG_INFO(TAG, "Resumed all downloads");
}

void DownloadManager::moveUp(DownloadId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (size_t i = 1; i < queue_.size(); ++i) {
        if (queue_[i] == id) {
            std::swap(queue_[i], queue_[i - 1]);
            break;
        }
    }
}

void DownloadManager::moveDown(DownloadId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (size_t i = 0; i + 1 < queue_.size(); ++i) {
        if (queue_[i] == id) {
            std::swap(queue_[i], queue_[i + 1]);
            break;
        }
    }
}

void DownloadManager::processQueue() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& settings = Settings::instance();
    int maxActive = settings.maxActiveDownloads();

    int current = 0;
    for (auto& [id, task] : tasks_) {
        if (task->isActive()) current++;
    }

    // Sort queue by priority
    std::stable_sort(queue_.begin(), queue_.end(), [this](DownloadId a, DownloadId b) {
        auto infoA = downloads_.find(a);
        auto infoB = downloads_.find(b);
        if (infoA == downloads_.end() || infoB == downloads_.end()) return false;
        return static_cast<int>(infoA->second->priority) >
               static_cast<int>(infoB->second->priority);
    });

    for (auto& id : queue_) {
        if (current >= maxActive) break;

        auto taskIt = tasks_.find(id);
        if (taskIt == tasks_.end()) continue;

        auto& task = taskIt->second;
        auto status = task->status();

        if (status == DownloadStatus::PENDING || status == DownloadStatus::QUEUED) {
            task->start();
            current++;
            LOG_INFO(TAG, "Queue: started download #" + std::to_string(id));
        }
    }
}

std::shared_ptr<DownloadInfo> DownloadManager::getDownload(DownloadId id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = downloads_.find(id);
    return (it != downloads_.end()) ? it->second : nullptr;
}

std::vector<std::shared_ptr<DownloadInfo>> DownloadManager::getDownloads(
    const DownloadFilter& filter) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<DownloadInfo>> result;

    for (auto& id : queue_) {
        auto it = downloads_.find(id);
        if (it == downloads_.end()) continue;
        auto& info = it->second;

        // Apply filters
        if (static_cast<int>(filter.statusFilter) >= 0 &&
            info->status != filter.statusFilter) continue;

        if (static_cast<int>(filter.categoryFilter) >= 0 &&
            info->category != filter.categoryFilter) continue;

        if (!filter.searchText.empty()) {
            // Case-insensitive search
            std::string lower = info->filename;
            std::string search = filter.searchText;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            std::transform(search.begin(), search.end(), search.begin(), ::tolower);
            if (lower.find(search) == std::string::npos) continue;
        }

        result.push_back(info);
    }

    return result;
}

int DownloadManager::activeCount() const {
    int count = 0;
    for (auto& [id, task] : tasks_) {
        if (task->isActive()) count++;
    }
    return count;
}

int DownloadManager::queuedCount() const {
    int count = 0;
    for (auto& [id, info] : downloads_) {
        if (info->status == DownloadStatus::QUEUED) count++;
    }
    return count;
}

bool DownloadManager::hasDuplicateUrl(const std::string& url) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [id, info] : downloads_) {
        if (info->url == url &&
            info->status != DownloadStatus::CANCELLED &&
            info->status != DownloadStatus::FAILED) {
            return true;
        }
    }
    return false;
}

int DownloadManager::recoverDownloads() {
    // TODO: Load from database and recover incomplete downloads
    LOG_INFO(TAG, "Checking for recoverable downloads...");
    return 0;
}

DownloadTask* DownloadManager::findTask(DownloadId id) {
    auto it = tasks_.find(id);
    return (it != tasks_.end()) ? it->second.get() : nullptr;
}

} // namespace ps5dm
