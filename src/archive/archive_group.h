#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Archive Group
// ═══════════════════════════════════════════════════════════════════════════════
#include "common/types.h"
#include "archive/archive_detector.h"
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace ps5dm {

/// Represents a part of a multipart archive
struct ArchivePart {
    int partIndex = 0;
    std::string filename;
    std::string filePath;
    FileSize fileSize = 0;
    bool found = false;
};

/// Represents a group of archive parts (or a single archive)
struct ArchiveGroup {
    ArchiveGroupId id = 0;
    std::string baseName;
    ArchiveType archiveType = ArchiveType::UNKNOWN;
    std::string directory;
    std::vector<ArchivePart> parts;
    std::string firstPartPath;    // the file to start extraction from
    int expectedParts = 0;
    ArchiveGroupStatus status = ArchiveGroupStatus::UNKNOWN;
    FileSize totalSize = 0;
    std::string destination;
    Error lastError;

    /// Get list of missing part indices
    std::vector<int> missingParts() const;

    /// Get found part count
    int foundPartCount() const;

    /// Is the archive complete (all parts present)?
    bool isComplete() const;

    /// Is this a single file archive (not multipart)?
    bool isSingleFile() const;
};

/// Manages grouping of archive files
class ArchiveGrouper {
public:
    /// Scan a list of archive infos and group multipart archives
    std::vector<std::shared_ptr<ArchiveGroup>> groupArchives(
        const std::vector<ArchiveInfo>& archives);

    /// Add a single archive info to existing groups
    void addArchive(const ArchiveInfo& info);

    /// Get all groups
    const std::vector<std::shared_ptr<ArchiveGroup>>& groups() const { return groups_; }

    /// Find group by ID
    std::shared_ptr<ArchiveGroup> findGroup(ArchiveGroupId id) const;

    /// Find group by base name
    std::shared_ptr<ArchiveGroup> findGroupByName(const std::string& baseName) const;

    /// Clear all groups
    void clear();

private:
    ArchiveGroupId nextId_ = 1;
    std::vector<std::shared_ptr<ArchiveGroup>> groups_;
    std::map<std::string, std::shared_ptr<ArchiveGroup>> groupMap_;  // groupKey → group

    void determineFirstPart(ArchiveGroup& group);
    void updateGroupStatus(ArchiveGroup& group);
};

} // namespace ps5dm
