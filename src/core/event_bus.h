#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Event Bus
// ═══════════════════════════════════════════════════════════════════════════════
#include "common/types.h"
#include <functional>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <any>
#include <string>

namespace ps5dm {

// ─── Event types ────────────────────────────────────────────────────────────
enum class EventType : int {
    // Download events
    DOWNLOAD_ADDED = 100,
    DOWNLOAD_STARTED,
    DOWNLOAD_PROGRESS,
    DOWNLOAD_PAUSED,
    DOWNLOAD_RESUMED,
    DOWNLOAD_COMPLETED,
    DOWNLOAD_FAILED,
    DOWNLOAD_RETRY,
    DOWNLOAD_CANCELLED,
    DOWNLOAD_REMOVED,
    DOWNLOAD_SPEED_UPDATE,
    DOWNLOAD_SEGMENT_COMPLETED,
    DOWNLOAD_QUEUE_CHANGED,
    DOWNLOAD_VERIFYING,
    DOWNLOAD_VERIFIED,

    // Extraction events
    EXTRACTION_STARTED = 200,
    EXTRACTION_PROGRESS,
    EXTRACTION_COMPLETED,
    EXTRACTION_FAILED,
    EXTRACTION_CANCELLED,

    // Archive events
    ARCHIVE_DETECTED = 300,
    ARCHIVE_GROUP_UPDATED,
    ARCHIVE_SCAN_COMPLETED,

    // Storage events
    STORAGE_WARNING = 400,
    STORAGE_CRITICAL,

    // Network events
    NETWORK_CONNECTED = 500,
    NETWORK_DISCONNECTED,
    NETWORK_CHANGED,

    // Application events
    APP_SHUTDOWN = 600,
    APP_ERROR,

    // UI events
    UI_NOTIFICATION = 700,
    UI_REFRESH
};

// ─── Event data structures ──────────────────────────────────────────────────

struct DownloadProgressEvent {
    DownloadId downloadId;
    FileSize downloadedBytes;
    FileSize totalBytes;
    FileSize speed;  // bytes/sec
    float percentage;
};

struct DownloadStatusEvent {
    DownloadId downloadId;
    DownloadStatus status;
    std::string filename;
    Error error;
};

struct SegmentProgressEvent {
    DownloadId downloadId;
    int segmentIndex;
    FileSize downloadedBytes;
    FileSize totalBytes;
    SegmentStatus status;
};

struct ExtractionProgressEvent {
    ArchiveGroupId groupId;
    std::string archivePath;
    int filesExtracted;
    int totalFiles;
    FileSize bytesExtracted;
    FileSize totalBytes;
    float percentage;
    std::string currentFile;
};

struct ExtractionStatusEvent {
    ArchiveGroupId groupId;
    std::string archivePath;
    ArchiveGroupStatus status;
    Error error;
};

struct StorageEvent {
    FileSize freeSpace;
    FileSize threshold;
    std::string path;
};

struct NotificationEvent {
    std::string title;
    std::string message;
    LogLevel severity;
};

// ─── Generic event wrapper ──────────────────────────────────────────────────
struct Event {
    EventType type;
    std::any data;
    Timestamp timestamp;

    Event() : type(EventType::APP_ERROR), timestamp(std::chrono::system_clock::now()) {}

    Event(EventType t, std::any d = {})
        : type(t), data(std::move(d)), timestamp(std::chrono::system_clock::now()) {}

    template<typename T>
    const T& get() const { return std::any_cast<const T&>(data); }

    template<typename T>
    T& get() { return std::any_cast<T&>(data); }
};

// ─── Event Bus ──────────────────────────────────────────────────────────────

using EventHandler = std::function<void(const Event&)>;
using HandlerId = uint64_t;

/// Thread-safe publish/subscribe event bus
class EventBus {
public:
    static EventBus& instance();

    /// Subscribe to an event type
    HandlerId subscribe(EventType type, EventHandler handler);

    /// Subscribe to all events
    HandlerId subscribeAll(EventHandler handler);

    /// Unsubscribe a handler
    void unsubscribe(HandlerId id);

    /// Publish an event synchronously (calls handlers on the publishing thread)
    void publish(const Event& event);

    /// Convenience publish
    void publish(EventType type) { publish(Event(type)); }

    template<typename T>
    void publish(EventType type, T&& data) {
        publish(Event(type, std::forward<T>(data)));
    }

    /// Queue event for deferred processing on main thread
    void queueEvent(const Event& event);

    /// Process queued events (call from main/UI thread)
    void processQueue();

    /// Clear all handlers
    void clear();

    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

private:
    EventBus() = default;

    struct Subscription {
        HandlerId id;
        EventHandler handler;
    };

    std::mutex mutex_;
    std::mutex queueMutex_;
    std::unordered_map<EventType, std::vector<Subscription>> handlers_;
    std::vector<Subscription> globalHandlers_;
    std::vector<Event> eventQueue_;
    HandlerId nextId_ = 1;
};

} // namespace ps5dm
