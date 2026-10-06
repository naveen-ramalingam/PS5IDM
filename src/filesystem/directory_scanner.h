#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Directory Scanner
// ═══════════════════════════════════════════════════════════════════════════════
#include "common/types.h"
#include <string>
#include <vector>

namespace ps5dm {

/// Recursively scans directories for files
class DirectoryScanner {
public:
    /// Scan directory and collect all file paths
    static void scan(const std::string& directory, std::vector<std::string>& files,
                     bool recursive = true, int maxDepth = 100);

    /// Scan directory and collect files matching extensions
    static void scanForExtensions(const std::string& directory,
                                   const std::vector<std::string>& extensions,
                                   std::vector<std::string>& files,
                                   bool recursive = true);

    /// Scan for archive files only
    static void scanForArchives(const std::string& directory,
                                 std::vector<std::string>& files,
                                 bool recursive = true);

    /// Count files in directory
    static int countFiles(const std::string& directory, bool recursive = true);
};

} // namespace ps5dm
