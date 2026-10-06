// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Download Segment Implementation
// ═══════════════════════════════════════════════════════════════════════════════
#include "downloader/segment.h"
#include "core/logger.h"
#include <algorithm>
#include <cmath>

namespace ps5dm {

static const char* TAG = "Segment";

void SegmentScheduler::createSegments(FileSize fileSize, FileSize segmentSize, int maxSegments) {
    std::lock_guard<std::mutex> lock(mutex_);
    segments_.clear();

    if (fileSize <= 0) {
        // Unknown size - single segment
        Segment seg;
        seg.index = 0;
        seg.startOffset = 0;
        seg.endOffset = -1;  // unknown
        seg.status = SegmentStatus::PENDING;
        segments_.push_back(seg);
        return;
    }

    // Adjust segment size to avoid too many segments
    FileSize adjustedSize = segmentSize;
    auto numSegments = static_cast<int>((fileSize + adjustedSize - 1) / adjustedSize);
    if (numSegments > maxSegments) {
        adjustedSize = (fileSize + maxSegments - 1) / maxSegments;
        numSegments = static_cast<int>((fileSize + adjustedSize - 1) / adjustedSize);
    }

    segments_.reserve(static_cast<size_t>(numSegments));

    FileOffset offset = 0;
    for (int i = 0; i < numSegments; ++i) {
        Segment seg;
        seg.index = i;
        seg.startOffset = offset;
        seg.endOffset = std::min(offset + adjustedSize - 1, fileSize - 1);
        seg.status = SegmentStatus::PENDING;
        segments_.push_back(seg);

        offset = seg.endOffset + 1;
    }

    LOG_DEBUG(TAG, "Created " + std::to_string(numSegments) + " segments of ~" +
              formatSize(adjustedSize) + " for " + formatSize(fileSize) + " file");
}

void SegmentScheduler::loadSegments(const std::vector<Segment>& segments) {
    std::lock_guard<std::mutex> lock(mutex_);
    segments_ = segments;
}

Segment* SegmentScheduler::getNextSegment() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& seg : segments_) {
        if (seg.status == SegmentStatus::PENDING) {
            seg.status = SegmentStatus::DOWNLOADING;
            return &seg;
        }
    }
    return nullptr;
}

Segment* SegmentScheduler::splitLargestRunning(FileSize minSplitSize) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Find the segment with the largest remaining bytes
    Segment* largest = nullptr;
    FileSize largestRemaining = 0;

    for (auto& seg : segments_) {
        if (seg.status == SegmentStatus::DOWNLOADING) {
            FileSize rem = seg.remaining();
            if (rem > largestRemaining && rem >= minSplitSize * 2) {
                largest = &seg;
                largestRemaining = rem;
            }
        }
    }

    if (!largest) return nullptr;

    // Split: take the second half of the largest segment
    FileOffset currentPos = largest->startOffset + largest->downloadedBytes;
    FileOffset midpoint = currentPos + (largest->endOffset - currentPos) / 2;

    // Create new segment for the second half
    Segment newSeg;
    newSeg.index = static_cast<int>(segments_.size());
    newSeg.startOffset = midpoint + 1;
    newSeg.endOffset = largest->endOffset;
    newSeg.status = SegmentStatus::DOWNLOADING;

    // Shrink the original
    largest->endOffset = midpoint;

    segments_.push_back(newSeg);

    LOG_DEBUG(TAG, "Split segment " + std::to_string(largest->index) +
              ", new segment " + std::to_string(newSeg.index));

    return &segments_.back();
}

void SegmentScheduler::completeSegment(int index) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& seg : segments_) {
        if (seg.index == index) {
            seg.status = SegmentStatus::COMPLETED;
            seg.downloadedBytes = seg.size();
            return;
        }
    }
}

void SegmentScheduler::failSegment(int index) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& seg : segments_) {
        if (seg.index == index) {
            seg.status = SegmentStatus::FAILED;
            return;
        }
    }
}

void SegmentScheduler::retrySegment(int index) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& seg : segments_) {
        if (seg.index == index) {
            seg.status = SegmentStatus::PENDING;
            seg.retryCount++;
            return;
        }
    }
}

FileSize SegmentScheduler::totalDownloaded() const {
    std::lock_guard<std::mutex> lock(mutex_);
    FileSize total = 0;
    for (auto& seg : segments_) {
        total += seg.downloadedBytes;
    }
    return total;
}

FileSize SegmentScheduler::totalSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    FileSize total = 0;
    for (auto& seg : segments_) {
        if (seg.endOffset >= 0) {
            total += seg.size();
        }
    }
    return total;
}

float SegmentScheduler::totalProgress() const {
    auto total = totalSize();
    if (total <= 0) return 0.0f;
    return static_cast<float>(totalDownloaded()) / static_cast<float>(total);
}

int SegmentScheduler::completedCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    int count = 0;
    for (auto& seg : segments_) {
        if (seg.status == SegmentStatus::COMPLETED) count++;
    }
    return count;
}

int SegmentScheduler::failedCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    int count = 0;
    for (auto& seg : segments_) {
        if (seg.status == SegmentStatus::FAILED) count++;
    }
    return count;
}

int SegmentScheduler::pendingCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    int count = 0;
    for (auto& seg : segments_) {
        if (seg.status == SegmentStatus::PENDING) count++;
    }
    return count;
}

int SegmentScheduler::activeCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    int count = 0;
    for (auto& seg : segments_) {
        if (seg.status == SegmentStatus::DOWNLOADING) count++;
    }
    return count;
}

bool SegmentScheduler::allComplete() const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& seg : segments_) {
        if (seg.status != SegmentStatus::COMPLETED) return false;
    }
    return !segments_.empty();
}

bool SegmentScheduler::hasFailed() const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& seg : segments_) {
        if (seg.status == SegmentStatus::FAILED) return true;
    }
    return false;
}

void SegmentScheduler::resetAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& seg : segments_) {
        seg.status = SegmentStatus::PENDING;
        seg.downloadedBytes = 0;
        seg.retryCount = 0;
    }
}

} // namespace ps5dm
