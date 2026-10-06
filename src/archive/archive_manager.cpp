// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Archive Manager Implementation
// ═══════════════════════════════════════════════════════════════════════════════
#include "archive/archive_manager.h"
#include "filesystem/directory_scanner.h"
#include "core/logger.h"
#include "core/event_bus.h"
#include "settings/settings.h"
#include "platform/paths.h"

namespace ps5dm {

static const char* TAG = "ArchiveMgr";

ArchiveManager& ArchiveManager::instance() {
    static ArchiveManager mgr;
    return mgr;
}

std::vector<std::shared_ptr<ArchiveGroup>> ArchiveManager::scanDirectory(
    const std::string& directory, bool recursive) {

    std::lock_guard<std::mutex> lock(mutex_);

    LOG_INFO(TAG, "Scanning for archives: " + directory +
             (recursive ? " (recursive)" : ""));

    lastScanDir_ = directory;

    // Use directory scanner to find all files
    std::vector<std::string> files;
    DirectoryScanner::scan(directory, files, recursive);

    // Detect archives
    std::vector<ArchiveInfo> archives;
    for (auto& file : files) {
        auto info = ArchiveDetector::detect(file);
        if (info.type != ArchiveType::UNKNOWN) {
            archives.push_back(info);
        }
    }

    LOG_INFO(TAG, "Found " + std::to_string(archives.size()) + " archive files");

    // Group them
    auto groups = grouper_.groupArchives(archives);

    LOG_INFO(TAG, "Grouped into " + std::to_string(groups.size()) + " archive groups");

    for (auto& group : groups) {
        LOG_DEBUG(TAG, "  " + group->baseName + " [" +
                  archiveTypeToString(group->archiveType) + "] " +
                  std::to_string(group->foundPartCount()) + "/" +
                  std::to_string(group->expectedParts) + " parts - " +
                  archiveGroupStatusToString(group->status));
    }

    EventBus::instance().queueEvent(Event(EventType::ARCHIVE_SCAN_COMPLETED));
    return groups;
}

const std::vector<std::shared_ptr<ArchiveGroup>>& ArchiveManager::groups() const {
    return grouper_.groups();
}

std::shared_ptr<ArchiveGroup> ArchiveManager::findGroup(ArchiveGroupId id) const {
    return grouper_.findGroup(id);
}

void ArchiveManager::extractGroup(ArchiveGroupId id, const std::string& destination, const std::string& password) {
    auto group = findGroup(id);
    if (!group) {
        LOG_ERROR(TAG, "Archive group not found: " + std::to_string(id));
        return;
    }

    if (group->status != ArchiveGroupStatus::READY) {
        LOG_WARN(TAG, "Archive group not ready: " + group->baseName);
        return;
    }

    std::string dest = destination.empty() ?
        Paths::join(group->directory, group->baseName) : destination;

    std::vector<std::shared_ptr<ArchiveGroup>> single = {group};
    batchExtractor_.extractGroups(single, dest, password,
        Settings::instance().overwritePolicy(),
        Settings::instance().parallelExtractionJobs());
}

void ArchiveManager::extractAllReady(const std::string& destination) {
    std::vector<std::shared_ptr<ArchiveGroup>> ready;
    for (auto& group : grouper_.groups()) {
        if (group->status == ArchiveGroupStatus::READY) {
            ready.push_back(group);
        }
    }

    if (ready.empty()) {
        LOG_INFO(TAG, "No ready archive groups to extract");
        return;
    }

    std::string dest = destination.empty() ?
        Settings::instance().extractionDirectory() : destination;

    batchExtractor_.extractGroups(ready, dest, "",
        Settings::instance().overwritePolicy(),
        Settings::instance().parallelExtractionJobs());
}

Result<void> ArchiveManager::testGroup(ArchiveGroupId id) {
    auto group = findGroup(id);
    if (!group) {
        return Result<void>::failure(Error::make(1, "Group not found"));
    }

    return Extractor::testArchive(group->firstPartPath);
}

void ArchiveManager::removeGroup(ArchiveGroupId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& groups = const_cast<std::vector<std::shared_ptr<ArchiveGroup>>&>(grouper_.groups());
    groups.erase(
        std::remove_if(groups.begin(), groups.end(),
            [id](const std::shared_ptr<ArchiveGroup>& g) { return g->id == id; }),
        groups.end()
    );
}

void ArchiveManager::rescan() {
    if (!lastScanDir_.empty()) {
        scanDirectory(lastScanDir_, Settings::instance().recursiveScanning());
    }
}

void ArchiveManager::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    grouper_.clear();
}

} // namespace ps5dm
