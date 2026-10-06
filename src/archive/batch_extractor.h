#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Batch Extractor
// ═══════════════════════════════════════════════════════════════════════════════
#include "common/types.h"
#include "archive/archive_group.h"
#include "archive/extractor.h"
#include <vector>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>

namespace ps5dm {

/// Result of a batch extraction
struct BatchExtractionResult {
    int totalGroups = 0;
    int successCount = 0;
    int failedCount = 0;
    int skippedCount = 0;
    std::vector<std::pair<std::string, Error>> failures;
};

/// Batch extractor for processing multiple archive groups
class BatchExtractor {
public:
    using GroupProgressCallback = std::function<void(ArchiveGroupId groupId,
                                                     const ExtractionProgress& progress)>;
    using BatchCompleteCallback = std::function<void(const BatchExtractionResult& result)>;

    BatchExtractor();
    ~BatchExtractor();

    /// Extract multiple archive groups
    void extractGroups(const std::vector<std::shared_ptr<ArchiveGroup>>& groups,
                       const std::string& defaultDestination,
                       const std::string& password = "",
                       OverwritePolicy overwrite = OverwritePolicy::ASK,
                       int parallelJobs = 1);

    /// Cancel batch extraction
    void cancel();

    /// Is extraction running?
    bool isRunning() const { return running_.load(); }

    /// Set callbacks
    void setProgressCallback(GroupProgressCallback cb) { progressCallback_ = std::move(cb); }
    void setCompleteCallback(BatchCompleteCallback cb) { completeCallback_ = std::move(cb); }

private:
    void extractionThread(std::vector<std::shared_ptr<ArchiveGroup>> groups,
                          std::string defaultDest, std::string password, OverwritePolicy overwrite, int jobs);

    GroupProgressCallback progressCallback_;
    BatchCompleteCallback completeCallback_;
    std::atomic<bool> running_{false};
    std::atomic<bool> cancelled_{false};
    std::thread thread_;
    std::mutex mutex_;
};

} // namespace ps5dm
