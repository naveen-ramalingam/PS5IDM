#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Common Types & Definitions
// ═══════════════════════════════════════════════════════════════════════════════

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <variant>
#include <atomic>
#include <mutex>

namespace ps5dm {

// ─── Basic type aliases ─────────────────────────────────────────────────────
using Byte = uint8_t;
using FileOffset = int64_t;
using FileSize = int64_t;
using DownloadId = uint64_t;
using ArchiveGroupId = uint64_t;
using Timestamp = std::chrono::system_clock::time_point;
using SteadyClock = std::chrono::steady_clock;
using SteadyTime = std::chrono::steady_clock::time_point;
using Duration = std::chrono::milliseconds;
using Seconds = std::chrono::seconds;

// ─── Constants ──────────────────────────────────────────────────────────────
constexpr FileSize KB = 1024;
constexpr FileSize MB = 1024 * KB;
constexpr FileSize GB = 1024 * MB;
constexpr FileSize TB = 1024 * GB;

constexpr int DEFAULT_CONNECTIONS = 8;
constexpr FileSize DEFAULT_SEGMENT_SIZE = 16 * MB;
constexpr FileSize DEFAULT_BUFFER_SIZE = 1 * MB;
constexpr int DEFAULT_MAX_RETRIES = 5;
constexpr int DEFAULT_MAX_ACTIVE_DOWNLOADS = 3;
constexpr int DEFAULT_CONNECT_TIMEOUT_SEC = 30;
constexpr int DEFAULT_TRANSFER_TIMEOUT_SEC = 300;
constexpr FileSize DEFAULT_LOW_STORAGE_THRESHOLD = 10 * GB;

constexpr const char* APP_NAME = "PS5 Download Manager";
constexpr const char* APP_VERSION = "1.0.0";

// ─── Log level ──────────────────────────────────────────────────────────────
enum class LogLevel : int {
    TRACE = 0,
    DEBUG = 1,
    INFO  = 2,
    WARN  = 3,
    ERR   = 4,
    FATAL = 5
};

const char* logLevelToString(LogLevel level);

// ─── Download status ────────────────────────────────────────────────────────
enum class DownloadStatus : int {
    PENDING = 0,
    QUEUED,
    CONNECTING,
    DOWNLOADING,
    PAUSED,
    COMPLETED,
    FAILED,
    CANCELLED,
    VERIFYING,
    RETRYING,
    MERGING
};

const char* downloadStatusToString(DownloadStatus s);

// ─── Download priority ──────────────────────────────────────────────────────
enum class DownloadPriority : int {
    LOWEST = 0,
    LOW = 1,
    NORMAL = 2,
    HIGH = 3,
    HIGHEST = 4
};

// ─── Segment status ─────────────────────────────────────────────────────────
enum class SegmentStatus : int {
    PENDING = 0,
    DOWNLOADING,
    COMPLETED,
    FAILED,
    RETRYING
};

// ─── Archive types ──────────────────────────────────────────────────────────
enum class ArchiveType : int {
    UNKNOWN = 0,
    ZIP,
    SEVENZIP,
    RAR,
    TAR,
    GZIP,
    BZIP2,
    XZ,
    ZSTD,
    TGZ,
    TBZ2,
    TXZ,
    ISO
};

const char* archiveTypeToString(ArchiveType t);

// ─── Archive group status ───────────────────────────────────────────────────
enum class ArchiveGroupStatus : int {
    UNKNOWN = 0,
    READY,
    MISSING_PARTS,
    CORRUPTED,
    EXTRACTING,
    COMPLETED,
    FAILED
};

const char* archiveGroupStatusToString(ArchiveGroupStatus s);

// ─── Overwrite policy ───────────────────────────────────────────────────────
enum class OverwritePolicy : int {
    ASK = 0,
    OVERWRITE,
    SKIP,
    RENAME_AUTO
};

// ─── Checksum algorithm ─────────────────────────────────────────────────────
enum class ChecksumAlgorithm : int {
    NONE = 0,
    MD5,
    SHA1,
    SHA256,
    SHA512
};

const char* checksumAlgorithmToString(ChecksumAlgorithm a);

// ─── Download category ──────────────────────────────────────────────────────
enum class DownloadCategory : int {
    OTHER = 0,
    GAMES,
    VIDEOS,
    MUSIC,
    ARCHIVES,
    DOCUMENTS,
    IMAGES
};

const char* downloadCategoryToString(DownloadCategory c);
DownloadCategory categorizeByExtension(const std::string& filename);

// ─── Error structure ────────────────────────────────────────────────────────
struct Error {
    int code = 0;
    std::string message;
    bool retryable = false;
    int httpStatus = 0;
    int systemError = 0;

    explicit operator bool() const { return code != 0; }
    static Error none() { return {}; }
    static Error make(int code, const std::string& msg, bool retryable = false) {
        Error e;
        e.code = code;
        e.message = msg;
        e.retryable = retryable;
        return e;
    }
    static Error http(int status, const std::string& msg) {
        Error e;
        e.code = status;
        e.httpStatus = status;
        e.message = msg;
        e.retryable = (status == 408 || status == 429 || status >= 500);
        return e;
    }
};

// ─── Result type ────────────────────────────────────────────────────────────
template<typename T>
struct Result {
    std::optional<T> value;
    Error error;

    bool ok() const { return !error && value.has_value(); }
    const T& get() const { return *value; }
    T& get() { return *value; }

    static Result success(T val) {
        Result r;
        r.value = std::move(val);
        return r;
    }
    static Result failure(Error err) {
        Result r;
        r.error = std::move(err);
        return r;
    }
};

template<>
struct Result<void> {
    Error error;
    bool ok() const { return !error; }
    static Result success() { return {}; }
    static Result failure(Error err) { Result r; r.error = std::move(err); return r; }
};

// ─── Speed calculation helper ───────────────────────────────────────────────
struct SpeedTracker {
    static constexpr size_t WINDOW_SIZE = 30; // 30 samples for moving average

    struct Sample {
        SteadyTime time;
        FileSize bytes;
    };

    std::vector<Sample> samples;
    FileSize peakSpeed = 0;
    FileSize totalBytes = 0;
    SteadyTime startTime;
    mutable std::mutex mutex;

    SpeedTracker() : startTime(SteadyClock::now()) {}

    void addSample(FileSize bytesDownloaded) {
        std::lock_guard<std::mutex> lock(mutex);
        auto now = SteadyClock::now();
        samples.push_back({now, bytesDownloaded});
        totalBytes += bytesDownloaded;

        // Keep window limited
        while (samples.size() > WINDOW_SIZE) {
            samples.erase(samples.begin());
        }

        auto speed = currentSpeedLocked();
        if (speed > peakSpeed) peakSpeed = speed;
    }

    FileSize currentSpeed() const {
        std::lock_guard<std::mutex> lock(mutex);
        return currentSpeedLocked();
    }

    FileSize averageSpeed() const {
        std::lock_guard<std::mutex> lock(mutex);
        auto elapsed = std::chrono::duration_cast<Seconds>(SteadyClock::now() - startTime);
        if (elapsed.count() <= 0) return 0;
        return totalBytes / elapsed.count();
    }

    Seconds estimatedTimeRemaining(FileSize remaining) const {
        auto speed = currentSpeed();
        if (speed <= 0) return Seconds(0);
        return Seconds(remaining / speed);
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex);
        samples.clear();
        peakSpeed = 0;
        totalBytes = 0;
        startTime = SteadyClock::now();
    }

private:
    FileSize currentSpeedLocked() const {
        if (samples.size() < 2) return 0;
        auto& first = samples.front();
        auto& last = samples.back();
        auto elapsed = std::chrono::duration_cast<Duration>(last.time - first.time);
        if (elapsed.count() <= 0) return 0;

        FileSize totalInWindow = 0;
        for (size_t i = 1; i < samples.size(); ++i) {
            totalInWindow += samples[i].bytes;
        }
        // bytes per second
        return (totalInWindow * 1000) / elapsed.count();
    }
};

// ─── Utility: format file size ──────────────────────────────────────────────
std::string formatSize(FileSize bytes);
std::string formatSpeed(FileSize bytesPerSecond);
std::string formatDuration(Seconds sec);
std::string formatTimestamp(Timestamp ts);

} // namespace ps5dm
