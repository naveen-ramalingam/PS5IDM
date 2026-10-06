#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - File Operations
// ═══════════════════════════════════════════════════════════════════════════════
#include "common/types.h"
#include <string>
#include <vector>

namespace ps5dm {

/// File entry information
struct FileEntry {
    std::string name;
    std::string path;
    FileSize size = 0;
    bool isDirectory = false;
    bool isArchive = false;
    ArchiveType archiveType = ArchiveType::UNKNOWN;
    Timestamp modifiedAt;
    std::string extension;
};

/// Low-level file operations
class FileOperations {
public:
    /// Check if file exists
    static bool exists(const std::string& path);

    /// Check if path is a directory
    static bool isDirectory(const std::string& path);

    /// Get file size
    static FileSize getFileSize(const std::string& path);

    /// Create directory (and parents)
    static Result<void> createDirectory(const std::string& path);

    /// Remove file
    static Result<void> removeFile(const std::string& path);

    /// Remove directory (recursively)
    static Result<void> removeDirectory(const std::string& path);

    /// Copy file
    static Result<void> copyFile(const std::string& src, const std::string& dst);

    /// Move file
    static Result<void> moveFile(const std::string& src, const std::string& dst);

    /// Rename file
    static Result<void> renameFile(const std::string& oldPath, const std::string& newPath);

    /// Get free space on the filesystem containing path
    static FileSize getFreeSpace(const std::string& path);

    /// Get total space on the filesystem containing path
    static FileSize getTotalSpace(const std::string& path);

    /// List directory contents
    static Result<std::vector<FileEntry>> listDirectory(const std::string& path);

    /// Get file properties
    static Result<FileEntry> getFileInfo(const std::string& path);
};

} // namespace ps5dm
