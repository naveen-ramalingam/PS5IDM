// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Archive Extractor Implementation
// ═══════════════════════════════════════════════════════════════════════════════
#include "archive/extractor.h"
#include "core/logger.h"
#include "platform/paths.h"

#ifndef NO_LIBARCHIVE
#include <archive.h>
#include <archive_entry.h>
#endif

#include <sys/stat.h>
#include <cstring>

namespace ps5dm {

static const char* TAG = "Extractor";

#ifndef NO_LIBARCHIVE

ExtractionResult Extractor::extract(const std::string& archivePath,
                                     const std::string& destination,
                                     const std::string& password,
                                     OverwritePolicy overwrite,
                                     ProgressCallback progress,
                                     CancelToken* cancel) {
    ExtractionResult result;
    result.destination = destination;

    LOG_INFO(TAG, "Extracting: " + archivePath + " → " + destination);

    // Create destination directory
    mkdir(destination.c_str(), 0755);

    struct archive* a = archive_read_new();
    struct archive* ext = archive_write_disk_new();

    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);
    
    if (!password.empty()) {
        archive_read_add_passphrase(a, password.c_str());
    }

    // Security: don't extract absolute paths, don't follow symlinks outside
    int flags = ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL |
                ARCHIVE_EXTRACT_FFLAGS | ARCHIVE_EXTRACT_SECURE_NODOTDOT |
                ARCHIVE_EXTRACT_SECURE_SYMLINKS;

    if (overwrite == OverwritePolicy::OVERWRITE) {
        flags |= ARCHIVE_EXTRACT_UNLINK;
    }

    archive_write_disk_set_options(ext, flags);
    archive_write_disk_set_standard_lookup(ext);

    int r = archive_read_open_filename(a, archivePath.c_str(), 10240);
    if (r != ARCHIVE_OK) {
        result.error = Error::make(1, "Cannot open archive: " +
                                   std::string(archive_error_string(a)));
        archive_read_free(a);
        archive_write_free(ext);
        return result;
    }

    struct archive_entry* entry;
    int totalFiles = 0;

    // First pass: count files (for progress)
    // Skip for large archives - just estimate
    // We'll update totalFiles as we go

    ExtractionProgress prog;
    prog.totalBytes = 0;  // Unknown until we process

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        if (cancel && cancel->load()) {
            LOG_INFO(TAG, "Extraction cancelled");
            result.error = Error::make(2, "Extraction cancelled by user");
            break;
        }

        const char* entryPath = archive_entry_pathname(entry);
        if (!entryPath) continue;

        std::string safePath = Paths::sanitize(entryPath);

        // Path traversal protection
        if (Paths::hasTraversal(entryPath)) {
            LOG_WARN(TAG, "Blocked path traversal: " + std::string(entryPath));
            result.failedFiles.push_back(entryPath);
            archive_read_data_skip(a);
            continue;
        }

        // Set extraction destination
        std::string fullPath = Paths::join(destination, safePath);
        archive_entry_set_pathname(entry, fullPath.c_str());

        // Skip if exists and policy says skip
        struct stat st;
        if (overwrite == OverwritePolicy::SKIP && stat(fullPath.c_str(), &st) == 0) {
            archive_read_data_skip(a);
            continue;
        }

        // Handle auto-rename
        if (overwrite == OverwritePolicy::RENAME_AUTO && stat(fullPath.c_str(), &st) == 0) {
            fullPath = Paths::makeUnique(fullPath);
            archive_entry_set_pathname(entry, fullPath.c_str());
        }

        r = archive_write_header(ext, entry);
        if (r != ARCHIVE_OK) {
            LOG_WARN(TAG, "Extract header error: " + std::string(archive_error_string(ext)));
            result.failedFiles.push_back(safePath);
        } else {
            // Copy data
            const void* buff;
            size_t size;
            la_int64_t offset;

            while (true) {
                r = archive_read_data_block(a, &buff, &size, &offset);
                if (r == ARCHIVE_EOF) break;
                if (r != ARCHIVE_OK) {
                    LOG_WARN(TAG, "Read error: " + std::string(archive_error_string(a)));
                    break;
                }
                r = archive_write_data_block(ext, buff, size, offset);
                if (r != ARCHIVE_OK) {
                    LOG_WARN(TAG, "Write error: " + std::string(archive_error_string(ext)));
                    break;
                }

                result.bytesExtracted += static_cast<FileSize>(size);
            }

            archive_write_finish_entry(ext);
            result.filesExtracted++;
            result.extractedFiles.push_back(safePath);
        }

        // Update progress
        if (progress) {
            prog.currentFile = safePath;
            prog.filesExtracted = result.filesExtracted;
            prog.bytesExtracted = result.bytesExtracted;
            progress(prog);
        }
    }

    archive_read_close(a);
    archive_read_free(a);
    archive_write_close(ext);
    archive_write_free(ext);

    if (!result.error && result.failedFiles.empty()) {
        result.success = true;
        LOG_INFO(TAG, "Extraction complete: " + std::to_string(result.filesExtracted) +
                 " files, " + formatSize(result.bytesExtracted));
    }

    return result;
}

Result<void> Extractor::testArchive(const std::string& archivePath,
                                     ProgressCallback progress) {
    struct archive* a = archive_read_new();
    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);

    int r = archive_read_open_filename(a, archivePath.c_str(), 10240);
    if (r != ARCHIVE_OK) {
        std::string err = archive_error_string(a);
        archive_read_free(a);
        return Result<void>::failure(Error::make(1, "Cannot open archive: " + err));
    }

    struct archive_entry* entry;
    int fileCount = 0;
    const void* buff;
    size_t size;
    la_int64_t offset;

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        // Read all data to verify CRC/integrity
        while (archive_read_data_block(a, &buff, &size, &offset) == ARCHIVE_OK) {
            // Just read through - libarchive checks CRCs
        }
        fileCount++;

        if (progress) {
            ExtractionProgress prog;
            prog.currentFile = archive_entry_pathname(entry);
            prog.filesExtracted = fileCount;
            progress(prog);
        }
    }

    int finalStatus = archive_errno(a);
    archive_read_close(a);
    archive_read_free(a);

    if (finalStatus != 0 && finalStatus != ARCHIVE_EOF) {
        return Result<void>::failure(Error::make(finalStatus, "Archive verification failed"));
    }

    LOG_INFO(TAG, "Archive test passed: " + archivePath + " (" +
             std::to_string(fileCount) + " files)");
    return Result<void>::success();
}

Result<std::vector<Extractor::ArchiveEntry>> Extractor::listContents(const std::string& archivePath) {
    std::vector<ArchiveEntry> entries;

    struct archive* a = archive_read_new();
    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);

    int r = archive_read_open_filename(a, archivePath.c_str(), 10240);
    if (r != ARCHIVE_OK) {
        std::string err = archive_error_string(a);
        archive_read_free(a);
        return Result<std::vector<ArchiveEntry>>::failure(Error::make(1, err));
    }

    struct archive_entry* entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        ArchiveEntry e;
        e.path = archive_entry_pathname(entry);
        e.size = archive_entry_size(entry);
        e.isDirectory = (archive_entry_filetype(entry) == AE_IFDIR);
        entries.push_back(e);
        archive_read_data_skip(a);
    }

    archive_read_close(a);
    archive_read_free(a);
    return Result<std::vector<ArchiveEntry>>::success(std::move(entries));
}

FileSize Extractor::estimateExtractedSize(const std::string& archivePath) {
    auto result = listContents(archivePath);
    if (!result.ok()) return 0;

    FileSize total = 0;
    for (auto& entry : result.get()) {
        total += entry.size;
    }
    return total;
}

#else // NO_LIBARCHIVE

ExtractionResult Extractor::extract(const std::string& archivePath,
                                     const std::string& destination,
                                     const std::string&, OverwritePolicy, ProgressCallback, CancelToken*) {
    ExtractionResult result;
    result.error = Error::make(1, "Archive extraction not available (libarchive not found)");
    LOG_ERROR(TAG, result.error.message);
    return result;
}

Result<void> Extractor::testArchive(const std::string&, ProgressCallback) {
    return Result<void>::failure(Error::make(1, "Archive testing not available"));
}

Result<std::vector<Extractor::ArchiveEntry>> Extractor::listContents(const std::string&) {
    return Result<std::vector<ArchiveEntry>>::failure(Error::make(1, "Not available"));
}

FileSize Extractor::estimateExtractedSize(const std::string&) { return 0; }

#endif // NO_LIBARCHIVE

} // namespace ps5dm
