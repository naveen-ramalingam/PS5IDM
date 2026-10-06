#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Download Segment
// ═══════════════════════════════════════════════════════════════════════════════
#include "common/types.h"
#include <vector>
#include <string>
#include <mutex>

namespace ps5dm {

/// Represents a byte range segment of a download
struct Segment {
    int index = 0;
    FileOffset startOffset = 0;
    FileOffset endOffset = 0;       // inclusive
    FileOffset downloadedBytes = 0;
    SegmentStatus status = SegmentStatus::PENDING;
    int retryCount = 0;

    FileSize size() const { return endOffset - startOffset + 1; }
    FileSize remaining() const { return size() - downloadedBytes; }
    float progress() const {
        auto s = size();
        return s > 0 ? static_cast<float>(downloadedBytes) / static_cast<float>(s) : 0.0f;
    }
    bool isComplete() const { return status == SegmentStatus::COMPLETED; }
};

/// Manages segment scheduling for a download
class SegmentScheduler {
public:
    /// Create segments for a file
    /// @param fileSize Total file size
    /// @param segmentSize Desired segment size
    /// @param maxSegments Maximum number of segments (to avoid millions for huge files)
    void createSegments(FileSize fileSize, FileSize segmentSize, int maxSegments = 1024);

    /// Load segments from saved state
    void loadSegments(const std::vector<Segment>& segments);

    /// Get next pending segment (thread-safe)
    /// Returns nullptr if no segments available
    Segment* getNextSegment();

    /// Get a segment that can be split from a running segment (dynamic scheduling)
    /// Returns nullptr if no segments can be split
    Segment* splitLargestRunning(FileSize minSplitSize);

    /// Mark segment as complete
    void completeSegment(int index);

    /// Mark segment as failed
    void failSegment(int index);

    /// Reset a failed segment for retry
    void retrySegment(int index);

    /// Get all segments
    const std::vector<Segment>& segments() const { return segments_; }
    std::vector<Segment>& segments() { return segments_; }

    /// Progress
    FileSize totalDownloaded() const;
    FileSize totalSize() const;
    float totalProgress() const;
    int completedCount() const;
    int failedCount() const;
    int pendingCount() const;
    int activeCount() const;
    bool allComplete() const;
    bool hasFailed() const;

    /// Reset all segments to pending
    void resetAll();

private:
    std::vector<Segment> segments_;
    mutable std::mutex mutex_;
};

} // namespace ps5dm
