#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Archive Detector
// ═══════════════════════════════════════════════════════════════════════════════
#include "common/types.h"
#include <string>
#include <vector>

namespace ps5dm {

/// Detected archive information
struct ArchiveInfo {
    std::string filePath;
    std::string filename;
    ArchiveType type = ArchiveType::UNKNOWN;
    FileSize fileSize = 0;
    bool isMultipart = false;
    int partNumber = -1;        // -1 if single or unknown
    std::string baseName;       // "Game" from "Game.part01.rar"
    std::string groupKey;       // normalized key for grouping
};

/// Detects archive files by extension and magic bytes
class ArchiveDetector {
public:
    /// Detect archive type from file path
    static ArchiveInfo detect(const std::string& filePath);

    /// Detect archive type from extension only
    static ArchiveType detectByExtension(const std::string& filename);

    /// Detect archive type from magic bytes
    static ArchiveType detectByMagicBytes(const std::string& filePath);

    /// Check if a file is an archive
    static bool isArchive(const std::string& filePath);

    /// Check if a filename looks like a multipart archive
    static bool isMultipart(const std::string& filename);

    /// Extract base name and part number from multipart filename
    /// Returns {baseName, partNumber, groupKey}
    struct MultipartInfo {
        std::string baseName;
        int partNumber = -1;
        std::string groupKey;
        ArchiveType type = ArchiveType::UNKNOWN;
    };
    static MultipartInfo parseMultipart(const std::string& filename);

    /// Get all supported archive extensions
    static std::vector<std::string> supportedExtensions();
};

} // namespace ps5dm
