// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Archive Group Implementation
// ═══════════════════════════════════════════════════════════════════════════════
#include "archive/archive_group.h"
#include "core/logger.h"
#include "platform/paths.h"
#include <algorithm>

namespace ps5dm {

static const char* TAG = "ArchiveGroup";

std::vector<int> ArchiveGroup::missingParts() const {
    std::vector<int> missing;
    for (auto& part : parts) {
        if (!part.found) {
            missing.push_back(part.partIndex);
        }
    }
    return missing;
}

int ArchiveGroup::foundPartCount() const {
    int count = 0;
    for (auto& part : parts) {
        if (part.found) count++;
    }
    return count;
}

bool ArchiveGroup::isComplete() const {
    if (parts.empty()) return false;
    for (auto& part : parts) {
        if (!part.found) return false;
    }
    return true;
}

bool ArchiveGroup::isSingleFile() const {
    return parts.size() == 1 && !parts[0].filename.empty();
}

// ─── Archive Grouper ────────────────────────────────────────────────────────

std::vector<std::shared_ptr<ArchiveGroup>> ArchiveGrouper::groupArchives(
    const std::vector<ArchiveInfo>& archives) {
    clear();

    for (auto& info : archives) {
        addArchive(info);
    }

    return groups_;
}

void ArchiveGrouper::addArchive(const ArchiveInfo& info) {
    if (info.isMultipart && !info.groupKey.empty()) {
        // Find or create group
        auto it = groupMap_.find(info.groupKey);
        std::shared_ptr<ArchiveGroup> group;

        if (it != groupMap_.end()) {
            group = it->second;
        } else {
            group = std::make_shared<ArchiveGroup>();
            group->id = nextId_++;
            group->baseName = info.baseName;
            group->archiveType = info.type;
            group->directory = Paths::dirname(info.filePath);
            groupMap_[info.groupKey] = group;
            groups_.push_back(group);
        }

        // Add/update part
        int partIdx = info.partNumber;
        if (partIdx < 0) partIdx = 0;

        // Ensure parts vector is large enough
        while (static_cast<int>(group->parts.size()) <= partIdx) {
            ArchivePart emptyPart;
            emptyPart.partIndex = static_cast<int>(group->parts.size());
            emptyPart.found = false;
            group->parts.push_back(emptyPart);
        }

        group->parts[static_cast<size_t>(partIdx)].partIndex = partIdx;
        group->parts[static_cast<size_t>(partIdx)].filename = info.filename;
        group->parts[static_cast<size_t>(partIdx)].filePath = info.filePath;
        group->parts[static_cast<size_t>(partIdx)].fileSize = info.fileSize;
        group->parts[static_cast<size_t>(partIdx)].found = true;

        group->totalSize += info.fileSize;
        group->expectedParts = static_cast<int>(group->parts.size());

        determineFirstPart(*group);
        updateGroupStatus(*group);

    } else {
        // Single file archive
        auto group = std::make_shared<ArchiveGroup>();
        group->id = nextId_++;
        group->baseName = info.baseName.empty() ? Paths::stem(info.filename) : info.baseName;
        group->archiveType = info.type;
        group->directory = Paths::dirname(info.filePath);
        group->expectedParts = 1;
        group->totalSize = info.fileSize;

        ArchivePart part;
        part.partIndex = 0;
        part.filename = info.filename;
        part.filePath = info.filePath;
        part.fileSize = info.fileSize;
        part.found = true;
        group->parts.push_back(part);

        group->firstPartPath = info.filePath;
        group->status = ArchiveGroupStatus::READY;

        groups_.push_back(group);
    }
}

void ArchiveGrouper::determineFirstPart(ArchiveGroup& group) {
    // The first part is typically:
    // RAR: .rar (not .r00), or .part01.rar (not .part02.rar)
    // 7Z: .7z.001
    // ZIP: .zip (not .z01)

    for (auto& part : group.parts) {
        if (part.found) {
            std::string lower = part.filename;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

            // RAR: main .rar file or .part01.rar
            if (group.archiveType == ArchiveType::RAR) {
                if (lower.find(".part01.rar") != std::string::npos ||
                    lower.find(".part1.rar") != std::string::npos ||
                    (Paths::extension(lower) == ".rar" &&
                     lower.find(".part") == std::string::npos &&
                     lower.find(".r0") == std::string::npos)) {
                    group.firstPartPath = part.filePath;
                    return;
                }
            }

            // 7Z: .7z.001
            if (group.archiveType == ArchiveType::SEVENZIP) {
                if (lower.find(".7z.001") != std::string::npos) {
                    group.firstPartPath = part.filePath;
                    return;
                }
            }

            // ZIP: .zip (not .z01)
            if (group.archiveType == ArchiveType::ZIP) {
                if (Paths::extension(lower) == ".zip") {
                    group.firstPartPath = part.filePath;
                    return;
                }
            }
        }
    }

    // Fallback: use the first found part
    for (auto& part : group.parts) {
        if (part.found) {
            group.firstPartPath = part.filePath;
            return;
        }
    }
}

void ArchiveGrouper::updateGroupStatus(ArchiveGroup& group) {
    if (group.parts.empty()) {
        group.status = ArchiveGroupStatus::UNKNOWN;
        return;
    }

    bool allFound = true;
    for (auto& part : group.parts) {
        if (!part.found) {
            allFound = false;
            break;
        }
    }

    if (allFound) {
        group.status = ArchiveGroupStatus::READY;
    } else {
        group.status = ArchiveGroupStatus::MISSING_PARTS;
    }
}

std::shared_ptr<ArchiveGroup> ArchiveGrouper::findGroup(ArchiveGroupId id) const {
    for (auto& g : groups_) {
        if (g->id == id) return g;
    }
    return nullptr;
}

std::shared_ptr<ArchiveGroup> ArchiveGrouper::findGroupByName(const std::string& baseName) const {
    for (auto& g : groups_) {
        if (g->baseName == baseName) return g;
    }
    return nullptr;
}

void ArchiveGrouper::clear() {
    groups_.clear();
    groupMap_.clear();
    nextId_ = 1;
}

} // namespace ps5dm
