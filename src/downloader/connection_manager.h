#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Connection Manager
// ═══════════════════════════════════════════════════════════════════════════════
#include "common/types.h"
#include <atomic>
#include <mutex>

namespace ps5dm {

/// Tracks and limits active connections and bandwidth
class ConnectionManager {
public:
    static ConnectionManager& instance();

    /// Register/unregister an active connection
    void addConnection();
    void removeConnection();
    int activeConnections() const { return activeConnections_.load(); }

    /// Global speed limiting
    void setSpeedLimit(FileSize bytesPerSec);
    FileSize speedLimit() const { return speedLimit_.load(); }

    /// Get per-connection speed limit (total / active connections)
    FileSize perConnectionLimit() const;

    /// Track bytes transferred for global speed calculation
    void addBytesTransferred(FileSize bytes);

    /// Get current global transfer speed (bytes/sec)
    FileSize currentSpeed() const;

    /// Reset counters
    void reset();

    ConnectionManager(const ConnectionManager&) = delete;
    ConnectionManager& operator=(const ConnectionManager&) = delete;

private:
    ConnectionManager() = default;

    std::atomic<int> activeConnections_{0};
    std::atomic<FileSize> speedLimit_{0};
    SpeedTracker speedTracker_;
};

} // namespace ps5dm
