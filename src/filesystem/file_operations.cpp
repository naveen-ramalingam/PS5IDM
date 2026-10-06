// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - File Operations Implementation
// ═══════════════════════════════════════════════════════════════════════════════
#include "filesystem/file_operations.h"
#include "archive/archive_detector.h"
#include "platform/paths.h"
#include "core/logger.h"

#include <sys/stat.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <cerrno>

namespace ps5dm {

static const char* TAG = "FileOps";

bool FileOperations::exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

bool FileOperations::isDirectory(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

FileSize FileOperations::getFileSize(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return -1;
    return static_cast<FileSize>(st.st_size);
}

Result<void> FileOperations::createDirectory(const std::string& path) {
    // Create parent directories recursively
    std::string current;
    for (size_t i = 0; i < path.size(); ++i) {
        current += path[i];
        if (path[i] == '/' || i == path.size() - 1) {
            if (!current.empty() && current != "/") {
                if (mkdir(current.c_str(), 0755) != 0 && errno != EEXIST) {
                    return Result<void>::failure(
                        Error::make(errno, "Failed to create directory: " + current +
                                   " - " + strerror(errno)));
                }
            }
        }
    }
    return Result<void>::success();
}

Result<void> FileOperations::removeFile(const std::string& path) {
    if (unlink(path.c_str()) != 0) {
        return Result<void>::failure(
            Error::make(errno, "Failed to delete: " + path + " - " + strerror(errno)));
    }
    return Result<void>::success();
}

Result<void> FileOperations::removeDirectory(const std::string& path) {
    DIR* dir = opendir(path.c_str());
    if (!dir) {
        return Result<void>::failure(Error::make(errno, "Cannot open directory: " + path));
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;

        std::string fullPath = Paths::join(path, name);

        struct stat st;
        if (stat(fullPath.c_str(), &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            auto result = removeDirectory(fullPath);
            if (!result.ok()) {
                closedir(dir);
                return result;
            }
        } else {
            if (unlink(fullPath.c_str()) != 0) {
                closedir(dir);
                return Result<void>::failure(
                    Error::make(errno, "Failed to delete: " + fullPath));
            }
        }
    }

    closedir(dir);

    if (rmdir(path.c_str()) != 0) {
        return Result<void>::failure(
            Error::make(errno, "Failed to remove directory: " + path));
    }

    return Result<void>::success();
}

Result<void> FileOperations::copyFile(const std::string& src, const std::string& dst) {
    std::ifstream in(src, std::ios::binary);
    if (!in.is_open()) {
        return Result<void>::failure(Error::make(1, "Cannot open source: " + src));
    }

    // Create parent directory
    auto parentDir = Paths::dirname(dst);
    createDirectory(parentDir);

    std::ofstream out(dst, std::ios::binary);
    if (!out.is_open()) {
        return Result<void>::failure(Error::make(1, "Cannot create destination: " + dst));
    }

    // Stream copy with buffer
    char buffer[65536];
    while (in.read(buffer, sizeof(buffer)) || in.gcount() > 0) {
        out.write(buffer, in.gcount());
        if (!out.good()) {
            return Result<void>::failure(Error::make(1, "Write error during copy"));
        }
    }

    return Result<void>::success();
}

Result<void> FileOperations::moveFile(const std::string& src, const std::string& dst) {
    // Try rename first (fast, same filesystem)
    if (rename(src.c_str(), dst.c_str()) == 0) {
        return Result<void>::success();
    }

    // If rename fails (cross-device), copy then delete
    auto copyResult = copyFile(src, dst);
    if (!copyResult.ok()) return copyResult;

    return removeFile(src);
}

Result<void> FileOperations::renameFile(const std::string& oldPath, const std::string& newPath) {
    if (rename(oldPath.c_str(), newPath.c_str()) != 0) {
        return Result<void>::failure(
            Error::make(errno, std::string("Rename failed: ") + strerror(errno)));
    }
    return Result<void>::success();
}

FileSize FileOperations::getFreeSpace(const std::string& path) {
    struct statvfs stat;
    if (statvfs(path.c_str(), &stat) != 0) return -1;
    return static_cast<FileSize>(stat.f_bavail) * static_cast<FileSize>(stat.f_frsize);
}

FileSize FileOperations::getTotalSpace(const std::string& path) {
    struct statvfs stat;
    if (statvfs(path.c_str(), &stat) != 0) return -1;
    return static_cast<FileSize>(stat.f_blocks) * static_cast<FileSize>(stat.f_frsize);
}

Result<std::vector<FileEntry>> FileOperations::listDirectory(const std::string& path) {
    std::vector<FileEntry> entries;

    DIR* dir = opendir(path.c_str());
    if (!dir) {
        return Result<std::vector<FileEntry>>::failure(
            Error::make(errno, "Cannot open directory: " + path));
    }

    struct dirent* d;
    while ((d = readdir(dir)) != nullptr) {
        std::string name = d->d_name;
        if (name == "." || name == "..") continue;

        FileEntry entry;
        entry.name = name;
        entry.path = Paths::join(path, name);
        entry.extension = Paths::extension(name);

        struct stat st;
        if (stat(entry.path.c_str(), &st) == 0) {
            entry.isDirectory = S_ISDIR(st.st_mode);
            entry.size = static_cast<FileSize>(st.st_size);
            entry.modifiedAt = std::chrono::system_clock::from_time_t(st.st_mtime);
        }

        // Check if archive
        if (!entry.isDirectory) {
            auto archType = ArchiveDetector::detectByExtension(name);
            if (archType != ArchiveType::UNKNOWN) {
                entry.isArchive = true;
                entry.archiveType = archType;
            }
        }

        entries.push_back(entry);
    }

    closedir(dir);
    return Result<std::vector<FileEntry>>::success(std::move(entries));
}

Result<FileEntry> FileOperations::getFileInfo(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return Result<FileEntry>::failure(
            Error::make(errno, "File not found: " + path));
    }

    FileEntry entry;
    entry.name = Paths::basename(path);
    entry.path = path;
    entry.size = static_cast<FileSize>(st.st_size);
    entry.isDirectory = S_ISDIR(st.st_mode);
    entry.extension = Paths::extension(path);
    entry.modifiedAt = std::chrono::system_clock::from_time_t(st.st_mtime);

    if (!entry.isDirectory) {
        auto archType = ArchiveDetector::detectByExtension(entry.name);
        if (archType != ArchiveType::UNKNOWN) {
            entry.isArchive = true;
            entry.archiveType = archType;
        }
    }

    return Result<FileEntry>::success(std::move(entry));
}

} // namespace ps5dm
