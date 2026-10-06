// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Directory Scanner Implementation
// ═══════════════════════════════════════════════════════════════════════════════
#include "filesystem/directory_scanner.h"
#include "archive/archive_detector.h"
#include "platform/paths.h"
#include "core/logger.h"

#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>

namespace ps5dm {

static const char* TAG = "DirScanner";

void DirectoryScanner::scan(const std::string& directory, std::vector<std::string>& files,
                             bool recursive, int maxDepth) {
    if (maxDepth <= 0) return;

    DIR* dir = opendir(directory.c_str());
    if (!dir) {
        LOG_WARN(TAG, "Cannot open directory: " + directory);
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;

        // Skip hidden files/directories (starting with .)
        if (!name.empty() && name[0] == '.') continue;

        std::string fullPath = Paths::join(directory, name);

        struct stat st;
        if (stat(fullPath.c_str(), &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            if (recursive) {
                scan(fullPath, files, true, maxDepth - 1);
            }
        } else if (S_ISREG(st.st_mode)) {
            files.push_back(fullPath);
        }
    }

    closedir(dir);
}

void DirectoryScanner::scanForExtensions(const std::string& directory,
                                          const std::vector<std::string>& extensions,
                                          std::vector<std::string>& files,
                                          bool recursive) {
    std::vector<std::string> allFiles;
    scan(directory, allFiles, recursive);

    for (auto& file : allFiles) {
        std::string ext = Paths::extension(file);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        for (auto& target : extensions) {
            std::string targetLower = target;
            std::transform(targetLower.begin(), targetLower.end(), targetLower.begin(), ::tolower);
            if (ext == targetLower) {
                files.push_back(file);
                break;
            }
        }
    }
}

void DirectoryScanner::scanForArchives(const std::string& directory,
                                        std::vector<std::string>& files,
                                        bool recursive) {
    std::vector<std::string> allFiles;
    scan(directory, allFiles, recursive);

    for (auto& file : allFiles) {
        if (ArchiveDetector::isArchive(file)) {
            files.push_back(file);
        }
    }
}

int DirectoryScanner::countFiles(const std::string& directory, bool recursive) {
    std::vector<std::string> files;
    scan(directory, files, recursive);
    return static_cast<int>(files.size());
}

} // namespace ps5dm
