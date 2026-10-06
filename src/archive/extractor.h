#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Archive Extractor
// ═══════════════════════════════════════════════════════════════════════════════
#include "common/types.h"
#include "archive/archive_group.h"
#include <string>
#include <functional>
#include <atomic>

namespace ps5dm {

/// Extraction progress info
struct ExtractionProgress {
    std::string currentFile;
    int filesExtracted = 0;
    int totalFiles = 0;
    FileSize bytesExtracted = 0;
    FileSize totalBytes = 0;
    float percentage = 0.0f;
};

/// Extraction result
struct ExtractionResult {
    bool success = false;
    int filesExtracted = 0;
    FileSize bytesExtracted = 0;
    std::string destination;
    Error error;
    std::vector<std::string> extractedFiles;
    std::vector<std::string> failedFiles;
};

/// Single archive extractor using libarchive
class Extractor {
public:
    using ProgressCallback = std::function<void(const ExtractionProgress&)>;
    using CancelToken = std::atomic<bool>;

    /// Extract an archive to destination
    static ExtractionResult extract(const std::string& archivePath,
                                     const std::string& destination,
                                     const std::string& password = "",
                                     OverwritePolicy overwrite = OverwritePolicy::ASK,
                                     ProgressCallback progress = nullptr,
                                     CancelToken* cancel = nullptr);

    /// Test/verify an archive
    static Result<void> testArchive(const std::string& archivePath,
                                     ProgressCallback progress = nullptr);

    /// List contents of an archive
    struct ArchiveEntry {
        std::string path;
        FileSize size;
        bool isDirectory;
    };
    static Result<std::vector<ArchiveEntry>> listContents(const std::string& archivePath);

    /// Get total uncompressed size estimate
    static FileSize estimateExtractedSize(const std::string& archivePath);
};

} // namespace ps5dm
