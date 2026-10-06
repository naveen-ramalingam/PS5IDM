// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - File Manager Implementation
// ═══════════════════════════════════════════════════════════════════════════════
#include "filesystem/file_manager.h"
#include "core/logger.h"
#include "settings/settings.h"
#include "platform/paths.h"

#include <algorithm>

namespace ps5dm {

static const char* TAG = "FileMgr";

FileManager& FileManager::instance() {
    static FileManager mgr;
    return mgr;
}

FileManager::FileManager() {
    currentPath_ = Settings::instance().downloadDirectory();
}

Result<void> FileManager::navigateTo(const std::string& path) {
    if (!FileOperations::isDirectory(path)) {
        return Result<void>::failure(Error::make(1, "Not a directory: " + path));
    }

    auto result = FileOperations::listDirectory(path);
    if (!result.ok()) return Result<void>::failure(result.error);

    pushHistory(path);
    currentPath_ = path;
    entries_ = result.get();
    selected_.assign(entries_.size(), false);
    sort(sortMode_);

    LOG_DEBUG(TAG, "Navigated to: " + path + " (" +
              std::to_string(entries_.size()) + " entries)");
    return Result<void>::success();
}

Result<void> FileManager::navigateUp() {
    if (currentPath_ == "/" || currentPath_.empty()) {
        return Result<void>::success();
    }
    return navigateTo(Paths::dirname(currentPath_));
}

Result<void> FileManager::refresh() {
    return navigateTo(currentPath_);
}

void FileManager::sort(FileSortMode mode) {
    sortMode_ = mode;

    // Always put directories first
    std::stable_sort(entries_.begin(), entries_.end(),
        [mode](const FileEntry& a, const FileEntry& b) {
            // Directories always come first
            if (a.isDirectory != b.isDirectory) return a.isDirectory > b.isDirectory;

            switch (mode) {
                case FileSortMode::NAME_ASC:
                    return a.name < b.name;
                case FileSortMode::NAME_DESC:
                    return a.name > b.name;
                case FileSortMode::SIZE_ASC:
                    return a.size < b.size;
                case FileSortMode::SIZE_DESC:
                    return a.size > b.size;
                case FileSortMode::DATE_ASC:
                    return a.modifiedAt < b.modifiedAt;
                case FileSortMode::DATE_DESC:
                    return a.modifiedAt > b.modifiedAt;
                case FileSortMode::TYPE_ASC:
                    return a.extension < b.extension;
                case FileSortMode::TYPE_DESC:
                    return a.extension > b.extension;
            }
            return false;
        }
    );
}

std::vector<FileEntry> FileManager::search(const std::string& query) const {
    std::vector<FileEntry> results;
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    for (auto& entry : entries_) {
        std::string lowerName = entry.name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

        if (lowerName.find(lowerQuery) != std::string::npos) {
            results.push_back(entry);
        }
    }
    return results;
}

Result<void> FileManager::createFolder(const std::string& name) {
    std::string path = Paths::join(currentPath_, name);
    auto result = FileOperations::createDirectory(path);
    if (result.ok()) refresh();
    return result;
}

Result<void> FileManager::deleteSelected() {
    auto selected = selectedEntries();
    for (auto& entry : selected) {
        if (entry.isDirectory) {
            auto r = FileOperations::removeDirectory(entry.path);
            if (!r.ok()) return r;
        } else {
            auto r = FileOperations::removeFile(entry.path);
            if (!r.ok()) return r;
        }
    }
    return refresh();
}

Result<void> FileManager::copySelected(const std::string& destination) {
    auto selected = selectedEntries();
    for (auto& entry : selected) {
        std::string dst = Paths::join(destination, entry.name);
        if (entry.isDirectory) {
            // TODO: recursive copy
            LOG_WARN(TAG, "Directory copy not yet implemented");
        } else {
            auto r = FileOperations::copyFile(entry.path, dst);
            if (!r.ok()) return r;
        }
    }
    return Result<void>::success();
}

Result<void> FileManager::moveSelected(const std::string& destination) {
    auto selected = selectedEntries();
    for (auto& entry : selected) {
        std::string dst = Paths::join(destination, entry.name);
        auto r = FileOperations::moveFile(entry.path, dst);
        if (!r.ok()) return r;
    }
    return refresh();
}

Result<void> FileManager::renameEntry(int index, const std::string& newName) {
    if (index < 0 || index >= static_cast<int>(entries_.size())) {
        return Result<void>::failure(Error::make(1, "Invalid index"));
    }

    auto& entry = entries_[static_cast<size_t>(index)];
    std::string newPath = Paths::join(Paths::dirname(entry.path), newName);

    auto result = FileOperations::renameFile(entry.path, newPath);
    if (result.ok()) refresh();
    return result;
}

// ─── Selection ──────────────────────────────────────────────────────────────

void FileManager::selectItem(int index) {
    if (index >= 0 && index < static_cast<int>(selected_.size()))
        selected_[static_cast<size_t>(index)] = true;
}

void FileManager::deselectItem(int index) {
    if (index >= 0 && index < static_cast<int>(selected_.size()))
        selected_[static_cast<size_t>(index)] = false;
}

void FileManager::toggleItem(int index) {
    if (index >= 0 && index < static_cast<int>(selected_.size()))
        selected_[static_cast<size_t>(index)] = !selected_[static_cast<size_t>(index)];
}

void FileManager::selectAll() {
    std::fill(selected_.begin(), selected_.end(), true);
}

void FileManager::deselectAll() {
    std::fill(selected_.begin(), selected_.end(), false);
}

void FileManager::invertSelection() {
    for (size_t i = 0; i < selected_.size(); ++i) selected_[i] = !selected_[i];
}

bool FileManager::isSelected(int index) const {
    if (index < 0 || index >= static_cast<int>(selected_.size())) return false;
    return selected_[static_cast<size_t>(index)];
}

std::vector<int> FileManager::selectedIndices() const {
    std::vector<int> result;
    for (int i = 0; i < static_cast<int>(selected_.size()); ++i) {
        if (selected_[static_cast<size_t>(i)]) result.push_back(i);
    }
    return result;
}

std::vector<FileEntry> FileManager::selectedEntries() const {
    std::vector<FileEntry> result;
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
        if (i < static_cast<int>(selected_.size()) && selected_[static_cast<size_t>(i)]) {
            result.push_back(entries_[static_cast<size_t>(i)]);
        }
    }
    return result;
}

int FileManager::selectedCount() const {
    int count = 0;
    for (auto s : selected_) if (s) count++;
    return count;
}

// ─── Clipboard ──────────────────────────────────────────────────────────────

void FileManager::cut() {
    clipboard_.clear();
    for (auto& entry : selectedEntries()) {
        clipboard_.push_back(entry.path);
    }
    clipboardIsCut_ = true;
}

void FileManager::copy() {
    clipboard_.clear();
    for (auto& entry : selectedEntries()) {
        clipboard_.push_back(entry.path);
    }
    clipboardIsCut_ = false;
}

Result<void> FileManager::paste() {
    for (auto& src : clipboard_) {
        std::string name = Paths::basename(src);
        std::string dst = Paths::join(currentPath_, name);

        if (clipboardIsCut_) {
            auto r = FileOperations::moveFile(src, dst);
            if (!r.ok()) return r;
        } else {
            auto r = FileOperations::copyFile(src, dst);
            if (!r.ok()) return r;
        }
    }

    if (clipboardIsCut_) clipboard_.clear();
    return refresh();
}

// ─── Navigation history ─────────────────────────────────────────────────────

void FileManager::pushHistory(const std::string& path) {
    if (historyIndex_ >= 0 && historyIndex_ < static_cast<int>(history_.size()) &&
        history_[static_cast<size_t>(historyIndex_)] == path) {
        return;  // Same as current
    }

    // Truncate forward history
    if (historyIndex_ + 1 < static_cast<int>(history_.size())) {
        history_.resize(static_cast<size_t>(historyIndex_ + 1));
    }

    history_.push_back(path);
    historyIndex_ = static_cast<int>(history_.size()) - 1;
}

bool FileManager::canGoBack() const {
    return historyIndex_ > 0;
}

bool FileManager::canGoForward() const {
    return historyIndex_ + 1 < static_cast<int>(history_.size());
}

void FileManager::goBack() {
    if (!canGoBack()) return;
    historyIndex_--;
    navigateTo(history_[static_cast<size_t>(historyIndex_)]);
}

void FileManager::goForward() {
    if (!canGoForward()) return;
    historyIndex_++;
    navigateTo(history_[static_cast<size_t>(historyIndex_)]);
}

} // namespace ps5dm
