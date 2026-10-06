// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Connection Manager Implementation
// ═══════════════════════════════════════════════════════════════════════════════
#include "downloader/connection_manager.h"

namespace ps5dm {

ConnectionManager& ConnectionManager::instance() {
    static ConnectionManager mgr;
    return mgr;
}

void ConnectionManager::addConnection() {
    activeConnections_++;
}

void ConnectionManager::removeConnection() {
    int prev = activeConnections_.fetch_sub(1);
    if (prev <= 0) activeConnections_ = 0;
}

FileSize ConnectionManager::perConnectionLimit() const {
    auto limit = speedLimit_.load();
    if (limit <= 0) return 0;
    auto conns = activeConnections_.load();
    if (conns <= 0) return limit;
    return limit / conns;
}

void ConnectionManager::addBytesTransferred(FileSize bytes) {
    speedTracker_.addSample(bytes);
}

FileSize ConnectionManager::currentSpeed() const {
    return speedTracker_.currentSpeed();
}

void ConnectionManager::reset() {
    activeConnections_ = 0;
    speedLimit_ = 0;
    speedTracker_.reset();
}

void ConnectionManager::setSpeedLimit(FileSize bytesPerSec) {
    speedLimit_ = bytesPerSec;
}

} // namespace ps5dm
