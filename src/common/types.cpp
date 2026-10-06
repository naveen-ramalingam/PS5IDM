// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Common Types Implementation
// ═══════════════════════════════════════════════════════════════════════════════
#include "common/types.h"
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>

namespace ps5dm {

const char* downloadStatusToString(DownloadStatus s) {
    switch (s) {
        case DownloadStatus::PENDING:     return "PENDING";
        case DownloadStatus::QUEUED:      return "QUEUED";
        case DownloadStatus::CONNECTING:  return "CONNECTING";
        case DownloadStatus::DOWNLOADING: return "DOWNLOADING";
        case DownloadStatus::PAUSED:      return "PAUSED";
        case DownloadStatus::COMPLETED:   return "COMPLETED";
        case DownloadStatus::FAILED:      return "FAILED";
        case DownloadStatus::CANCELLED:   return "CANCELLED";
        case DownloadStatus::VERIFYING:   return "VERIFYING";
        case DownloadStatus::RETRYING:    return "RETRYING";
        case DownloadStatus::MERGING:     return "MERGING";
    }
    return "UNKNOWN";
}

const char* archiveTypeToString(ArchiveType t) {
    switch (t) {
        case ArchiveType::UNKNOWN:  return "UNKNOWN";
        case ArchiveType::ZIP:      return "ZIP";
        case ArchiveType::SEVENZIP: return "7Z";
        case ArchiveType::RAR:      return "RAR";
        case ArchiveType::TAR:      return "TAR";
        case ArchiveType::GZIP:     return "GZ";
        case ArchiveType::BZIP2:    return "BZ2";
        case ArchiveType::XZ:       return "XZ";
        case ArchiveType::ZSTD:     return "ZST";
        case ArchiveType::TGZ:      return "TGZ";
        case ArchiveType::TBZ2:     return "TBZ2";
        case ArchiveType::TXZ:      return "TXZ";
        case ArchiveType::ISO:      return "ISO";
    }
    return "UNKNOWN";
}

const char* archiveGroupStatusToString(ArchiveGroupStatus s) {
    switch (s) {
        case ArchiveGroupStatus::UNKNOWN:       return "UNKNOWN";
        case ArchiveGroupStatus::READY:         return "READY";
        case ArchiveGroupStatus::MISSING_PARTS: return "MISSING PARTS";
        case ArchiveGroupStatus::CORRUPTED:     return "CORRUPTED";
        case ArchiveGroupStatus::EXTRACTING:    return "EXTRACTING";
        case ArchiveGroupStatus::COMPLETED:     return "COMPLETED";
        case ArchiveGroupStatus::FAILED:        return "FAILED";
    }
    return "UNKNOWN";
}

const char* checksumAlgorithmToString(ChecksumAlgorithm a) {
    switch (a) {
        case ChecksumAlgorithm::NONE:   return "NONE";
        case ChecksumAlgorithm::MD5:    return "MD5";
        case ChecksumAlgorithm::SHA1:   return "SHA-1";
        case ChecksumAlgorithm::SHA256: return "SHA-256";
        case ChecksumAlgorithm::SHA512: return "SHA-512";
    }
    return "UNKNOWN";
}

const char* downloadCategoryToString(DownloadCategory c) {
    switch (c) {
        case DownloadCategory::OTHER:     return "Other";
        case DownloadCategory::GAMES:     return "Games";
        case DownloadCategory::VIDEOS:    return "Videos";
        case DownloadCategory::MUSIC:     return "Music";
        case DownloadCategory::ARCHIVES:  return "Archives";
        case DownloadCategory::DOCUMENTS: return "Documents";
        case DownloadCategory::IMAGES:    return "Images";
    }
    return "Other";
}

DownloadCategory categorizeByExtension(const std::string& filename) {
    // Find last dot
    auto dotPos = filename.rfind('.');
    if (dotPos == std::string::npos) return DownloadCategory::OTHER;

    std::string ext = filename.substr(dotPos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    // Games
    if (ext == "pkg" || ext == "fpkg" || ext == "xci" || ext == "nsp")
        return DownloadCategory::GAMES;

    // Videos
    if (ext == "mp4" || ext == "mkv" || ext == "avi" || ext == "mov" ||
        ext == "wmv" || ext == "flv" || ext == "webm" || ext == "m4v")
        return DownloadCategory::VIDEOS;

    // Music
    if (ext == "mp3" || ext == "flac" || ext == "wav" || ext == "aac" ||
        ext == "ogg" || ext == "m4a" || ext == "wma" || ext == "opus")
        return DownloadCategory::MUSIC;

    // Archives
    if (ext == "zip" || ext == "7z" || ext == "rar" || ext == "tar" ||
        ext == "gz" || ext == "bz2" || ext == "xz" || ext == "zst" ||
        ext == "tgz" || ext == "tbz2" || ext == "txz" || ext == "iso")
        return DownloadCategory::ARCHIVES;

    // Documents
    if (ext == "pdf" || ext == "doc" || ext == "docx" || ext == "txt" ||
        ext == "rtf" || ext == "xls" || ext == "xlsx" || ext == "ppt" ||
        ext == "pptx" || ext == "csv" || ext == "json" || ext == "xml")
        return DownloadCategory::DOCUMENTS;

    // Images
    if (ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "gif" ||
        ext == "bmp" || ext == "svg" || ext == "webp" || ext == "tiff")
        return DownloadCategory::IMAGES;

    return DownloadCategory::OTHER;
}

std::string formatSize(FileSize bytes) {
    if (bytes < 0) return "0 B";

    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIdx = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unitIdx < 4) {
        size /= 1024.0;
        unitIdx++;
    }

    std::ostringstream oss;
    if (unitIdx == 0) {
        oss << bytes << " B";
    } else {
        oss << std::fixed << std::setprecision(1) << size << " " << units[unitIdx];
    }
    return oss.str();
}

std::string formatSpeed(FileSize bytesPerSecond) {
    return formatSize(bytesPerSecond) + "/s";
}

std::string formatDuration(Seconds sec) {
    auto s = sec.count();
    if (s < 0) return "—";
    if (s == 0) return "0s";

    int hours = static_cast<int>(s / 3600);
    int minutes = static_cast<int>((s % 3600) / 60);
    int seconds = static_cast<int>(s % 60);

    std::ostringstream oss;
    if (hours > 0) oss << hours << "h ";
    if (minutes > 0) oss << minutes << "m ";
    oss << seconds << "s";
    return oss.str();
}

std::string formatTimestamp(Timestamp ts) {
    auto time = std::chrono::system_clock::to_time_t(ts);
    struct tm tm_buf;
    localtime_r(&time, &tm_buf);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
    return buf;
}

} // namespace ps5dm
