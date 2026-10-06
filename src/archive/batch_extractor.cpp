// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Batch Extractor Implementation
// ═══════════════════════════════════════════════════════════════════════════════
#include "archive/batch_extractor.h"
#include "core/logger.h"
#include "core/event_bus.h"
#include "platform/paths.h"
#include "settings/settings.h"

#include <queue>

namespace ps5dm {

static const char* TAG = "BatchExtract";

BatchExtractor::BatchExtractor() = default;

BatchExtractor::~BatchExtractor() {
    cancel();
    if (thread_.joinable()) thread_.join();
}

void BatchExtractor::extractGroups(const std::vector<std::shared_ptr<ArchiveGroup>>& groups,
                                    const std::string& defaultDestination,
                                    const std::string& password,
                                    OverwritePolicy overwrite, int parallelJobs) {
    if (running_.load()) {
        LOG_WARN(TAG, "Batch extraction already running");
        return;
    }

    cancelled_ = false;
    running_ = true;

    // Copy groups for thread
    if (thread_.joinable()) thread_.join();
    thread_ = std::thread(&BatchExtractor::extractionThread, this,
                          groups, defaultDestination, password, overwrite, parallelJobs);
}

void BatchExtractor::cancel() {
    cancelled_ = true;
}

void BatchExtractor::extractionThread(std::vector<std::shared_ptr<ArchiveGroup>> groups,
                                       std::string defaultDest, std::string password, OverwritePolicy overwrite,
                                       int jobs) {
    BatchExtractionResult batchResult;
    batchResult.totalGroups = static_cast<int>(groups.size());

    LOG_INFO(TAG, "Starting batch extraction: " + std::to_string(groups.size()) +
             " groups, " + std::to_string(jobs) + " parallel jobs");

    // Build work queue (only READY groups)
    std::queue<std::shared_ptr<ArchiveGroup>> workQueue;
    for (auto& group : groups) {
        if (group->status == ArchiveGroupStatus::READY) {
            workQueue.push(group);
        } else {
            batchResult.skippedCount++;
            LOG_WARN(TAG, "Skipping " + group->baseName + ": " +
                     archiveGroupStatusToString(group->status));
        }
    }

    // Process queue (sequential for now; parallel would use a thread pool)
    // For PS5, sequential is safer due to memory/IO constraints
    while (!workQueue.empty() && !cancelled_.load()) {
        auto group = workQueue.front();
        workQueue.pop();

        group->status = ArchiveGroupStatus::EXTRACTING;
        LOG_INFO(TAG, "Extracting: " + group->baseName +
                 " (" + std::to_string(group->expectedParts) + " parts)");

        // Determine destination
        std::string dest = group->destination.empty() ?
            Paths::join(defaultDest, group->baseName) : group->destination;

        // Extract using the first part path
        std::atomic<bool> cancelToken{false};

        auto result = Extractor::extract(
            group->firstPartPath, dest, password, overwrite,
            [this, &group](const ExtractionProgress& prog) {
                if (progressCallback_) {
                    progressCallback_(group->id, prog);
                }

                ExtractionProgressEvent evt;
                evt.groupId = group->id;
                evt.archivePath = group->firstPartPath;
                evt.filesExtracted = prog.filesExtracted;
                evt.totalFiles = prog.totalFiles;
                evt.bytesExtracted = prog.bytesExtracted;
                evt.totalBytes = prog.totalBytes;
                evt.percentage = prog.percentage;
                evt.currentFile = prog.currentFile;
                EventBus::instance().queueEvent(Event(EventType::EXTRACTION_PROGRESS, evt));
            },
            &cancelToken
        );

        if (cancelled_.load()) {
            cancelToken = true;
            group->status = ArchiveGroupStatus::FAILED;
            break;
        }

        if (result.success) {
            group->status = ArchiveGroupStatus::COMPLETED;
            batchResult.successCount++;

            LOG_INFO(TAG, "Extracted: " + group->baseName + " → " + dest +
                     " (" + std::to_string(result.filesExtracted) + " files)");

            // Delete archives after extraction if setting enabled
            if (Settings::instance().deleteArchiveAfterExtraction()) {
                for (auto& part : group->parts) {
                    if (part.found) {
                        if (remove(part.filePath.c_str()) == 0) {
                            LOG_DEBUG(TAG, "Deleted: " + part.filePath);
                        }
                    }
                }
            }

            EventBus::instance().queueEvent(Event(EventType::EXTRACTION_COMPLETED,
                ExtractionStatusEvent{group->id, group->firstPartPath,
                                      ArchiveGroupStatus::COMPLETED, Error::none()}));
        } else {
            group->status = ArchiveGroupStatus::FAILED;
            group->lastError = result.error;
            batchResult.failedCount++;
            batchResult.failures.emplace_back(group->baseName, result.error);

            LOG_ERROR(TAG, "Failed: " + group->baseName + " - " + result.error.message);

            EventBus::instance().queueEvent(Event(EventType::EXTRACTION_FAILED,
                ExtractionStatusEvent{group->id, group->firstPartPath,
                                      ArchiveGroupStatus::FAILED, result.error}));
        }
    }

    running_ = false;

    LOG_INFO(TAG, "Batch extraction complete: " +
             std::to_string(batchResult.successCount) + " success, " +
             std::to_string(batchResult.failedCount) + " failed, " +
             std::to_string(batchResult.skippedCount) + " skipped");

    if (completeCallback_) {
        completeCallback_(batchResult);
    }
}

} // namespace ps5dm
