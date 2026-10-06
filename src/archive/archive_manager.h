#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Archive Manager
// ═══════════════════════════════════════════════════════════════════════════════
#include "common/types.h"
#include "archive/archive_detector.h"
#include "archive/archive_group.h"
#include "archive/extractor.h"
#include "archive/batch_extractor.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>

namespace ps5dm {

/// High-level archive management
class ArchiveManager {
public:
    static ArchiveManager& instance();

    /// Scan a directory for archives (optionally recursive)
    std::vector<std::shared_ptr<ArchiveGroup>> scanDirectory(
        const std::string& directory, bool recursive = true);

    /// Get all known archive groups
    const std::vector<std::shared_ptr<ArchiveGroup>>& groups() const;

    /// Find a group by ID
    std::shared_ptr<ArchiveGroup> findGroup(ArchiveGroupId id) const;

    /// Extract a single archive group
    void extractGroup(ArchiveGroupId id, const std::string& destination = "", const std::string& password = "");

    /// Extract all ready groups
    void extractAllReady(const std::string& destination = "");

    /// Test an archive group
    Result<void> testGroup(ArchiveGroupId id);

    /// Remove a group
    void removeGroup(ArchiveGroupId id);

    /// Get batch extractor
    BatchExtractor& batchExtractor() { return batchExtractor_; }

    /// Rescan with current settings
    void rescan();

    /// Clear all groups
    void clear();

    ArchiveManager(const ArchiveManager&) = delete;
    ArchiveManager& operator=(const ArchiveManager&) = delete;

private:
    ArchiveManager() = default;

    ArchiveGrouper grouper_;
    BatchExtractor batchExtractor_;
    std::string lastScanDir_;
    mutable std::mutex mutex_;
};

} // namespace ps5dm
