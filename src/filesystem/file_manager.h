#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - File Manager
// ═══════════════════════════════════════════════════════════════════════════════
#include "common/types.h"
#include "filesystem/file_operations.h"
#include <string>
#include <vector>
#include <functional>

namespace ps5dm {

/// Sort mode for file listing
enum class FileSortMode : int {
    NAME_ASC = 0,
    NAME_DESC,
    SIZE_ASC,
    SIZE_DESC,
    DATE_ASC,
    DATE_DESC,
    TYPE_ASC,
    TYPE_DESC
};

/// High-level file manager for browsing and managing files
class FileManager {
public:
    static FileManager& instance();

    /// Navigate to a directory
    Result<void> navigateTo(const std::string& path);

    /// Go up one directory
    Result<void> navigateUp();

    /// Get current directory
    const std::string& currentPath() const { return currentPath_; }

    /// Get current entries
    const std::vector<FileEntry>& entries() const { return entries_; }

    /// Refresh current directory
    Result<void> refresh();

    /// Sort entries
    void sort(FileSortMode mode);

    /// Search within current directory
    std::vector<FileEntry> search(const std::string& query) const;

    /// File operations
    Result<void> createFolder(const std::string& name);
    Result<void> deleteSelected();
    Result<void> copySelected(const std::string& destination);
    Result<void> moveSelected(const std::string& destination);
    Result<void> renameEntry(int index, const std::string& newName);

    /// Selection
    void selectItem(int index);
    void deselectItem(int index);
    void toggleItem(int index);
    void selectAll();
    void deselectAll();
    void invertSelection();
    bool isSelected(int index) const;
    std::vector<int> selectedIndices() const;
    std::vector<FileEntry> selectedEntries() const;
    int selectedCount() const;

    /// Clipboard
    void cut();
    void copy();
    Result<void> paste();
    bool hasClipboard() const { return !clipboard_.empty(); }

    /// Navigation history
    bool canGoBack() const;
    bool canGoForward() const;
    void goBack();
    void goForward();

    FileManager(const FileManager&) = delete;
    FileManager& operator=(const FileManager&) = delete;

private:
    FileManager();

    std::string currentPath_;
    std::vector<FileEntry> entries_;
    std::vector<bool> selected_;
    FileSortMode sortMode_ = FileSortMode::NAME_ASC;

    // Clipboard
    std::vector<std::string> clipboard_;
    bool clipboardIsCut_ = false;

    // History
    std::vector<std::string> history_;
    int historyIndex_ = -1;
    void pushHistory(const std::string& path);
};

} // namespace ps5dm
