// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Archive Detector Implementation
// ═══════════════════════════════════════════════════════════════════════════════
#include "archive/archive_detector.h"
#include "core/logger.h"
#include "platform/paths.h"

#include <fstream>
#include <algorithm>
#include <regex>
#include <cstring>
#include <sys/stat.h>

namespace ps5dm {

static const char* TAG = "ArchiveDetect";

// Magic bytes signatures
static const uint8_t ZIP_MAGIC[]    = {0x50, 0x4B, 0x03, 0x04};
static const uint8_t ZIP_EMPTY[]    = {0x50, 0x4B, 0x05, 0x06};
static const uint8_t SEVENZIP_MAGIC[] = {0x37, 0x7A, 0xBC, 0xAF, 0x27, 0x1C};
static const uint8_t RAR_MAGIC[]    = {0x52, 0x61, 0x72, 0x21, 0x1A, 0x07};
static const uint8_t GZIP_MAGIC[]   = {0x1F, 0x8B};
static const uint8_t BZIP2_MAGIC[]  = {0x42, 0x5A, 0x68};
static const uint8_t XZ_MAGIC[]     = {0xFD, 0x37, 0x7A, 0x58, 0x5A, 0x00};
static const uint8_t ZSTD_MAGIC[]   = {0x28, 0xB5, 0x2F, 0xFD};
static const uint8_t ISO_MAGIC_OFFSET = 0x80;  // "CD001" at offset 0x8001

ArchiveType ArchiveDetector::detectByExtension(const std::string& filename) {
    std::string ext = Paths::extension(filename);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".zip" || ext == ".z01" || ext == ".z02")  return ArchiveType::ZIP;
    if (ext == ".7z")                                      return ArchiveType::SEVENZIP;
    if (ext == ".rar" || ext == ".r00" || ext == ".r01")   return ArchiveType::RAR;
    if (ext == ".tar")                                     return ArchiveType::TAR;
    if (ext == ".gz" || ext == ".gzip")                    return ArchiveType::GZIP;
    if (ext == ".bz2" || ext == ".bzip2")                  return ArchiveType::BZIP2;
    if (ext == ".xz")                                      return ArchiveType::XZ;
    if (ext == ".zst" || ext == ".zstd")                   return ArchiveType::ZSTD;
    if (ext == ".tgz" || ext == ".tar.gz")                 return ArchiveType::TGZ;
    if (ext == ".tbz2" || ext == ".tar.bz2")               return ArchiveType::TBZ2;
    if (ext == ".txz" || ext == ".tar.xz")                 return ArchiveType::TXZ;
    if (ext == ".iso")                                     return ArchiveType::ISO;

    // Numeric extensions for 7z/split archives
    if (ext.size() == 4 && ext[0] == '.' &&
        std::isdigit(static_cast<unsigned char>(ext[1])) &&
        std::isdigit(static_cast<unsigned char>(ext[2])) &&
        std::isdigit(static_cast<unsigned char>(ext[3]))) {
        // Check if the stem ends with .7z, .zip, etc.
        std::string stem = Paths::stem(filename);
        std::string stemExt = Paths::extension(stem);
        std::transform(stemExt.begin(), stemExt.end(), stemExt.begin(), ::tolower);
        if (stemExt == ".7z")  return ArchiveType::SEVENZIP;
        if (stemExt == ".zip") return ArchiveType::ZIP;
    }

    // RAR old-style: .r00, .r01, ...
    if (ext.size() == 4 && ext[0] == '.' && ext[1] == 'r' &&
        std::isdigit(static_cast<unsigned char>(ext[2])) &&
        std::isdigit(static_cast<unsigned char>(ext[3]))) {
        return ArchiveType::RAR;
    }

    return ArchiveType::UNKNOWN;
}

ArchiveType ArchiveDetector::detectByMagicBytes(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return ArchiveType::UNKNOWN;

    uint8_t header[16] = {};
    file.read(reinterpret_cast<char*>(header), sizeof(header));
    auto bytesRead = file.gcount();

    if (bytesRead < 2) return ArchiveType::UNKNOWN;

    // Check signatures
    if (bytesRead >= 4 && (memcmp(header, ZIP_MAGIC, 4) == 0 || memcmp(header, ZIP_EMPTY, 4) == 0))
        return ArchiveType::ZIP;

    if (bytesRead >= 6 && memcmp(header, SEVENZIP_MAGIC, 6) == 0)
        return ArchiveType::SEVENZIP;

    if (bytesRead >= 6 && memcmp(header, RAR_MAGIC, 6) == 0)
        return ArchiveType::RAR;

    if (bytesRead >= 6 && memcmp(header, XZ_MAGIC, 6) == 0)
        return ArchiveType::XZ;

    if (bytesRead >= 4 && memcmp(header, ZSTD_MAGIC, 4) == 0)
        return ArchiveType::ZSTD;

    if (bytesRead >= 3 && memcmp(header, BZIP2_MAGIC, 3) == 0)
        return ArchiveType::BZIP2;

    if (bytesRead >= 2 && memcmp(header, GZIP_MAGIC, 2) == 0)
        return ArchiveType::GZIP;

    // TAR: check for "ustar" at offset 257
    if (bytesRead >= 6) {
        file.seekg(257);
        char tarMagic[6] = {};
        file.read(tarMagic, 5);
        if (file.gcount() >= 5 && strncmp(tarMagic, "ustar", 5) == 0)
            return ArchiveType::TAR;
    }

    return ArchiveType::UNKNOWN;
}

ArchiveInfo ArchiveDetector::detect(const std::string& filePath) {
    ArchiveInfo info;
    info.filePath = filePath;
    info.filename = Paths::basename(filePath);

    // Get file size
    struct stat st;
    if (stat(filePath.c_str(), &st) == 0) {
        info.fileSize = st.st_size;
    }

    // Try extension first
    info.type = detectByExtension(info.filename);

    // If unknown, try magic bytes
    if (info.type == ArchiveType::UNKNOWN) {
        info.type = detectByMagicBytes(filePath);
    }

    // Check multipart
    auto mpInfo = parseMultipart(info.filename);
    info.isMultipart = (mpInfo.partNumber >= 0);
    info.partNumber = mpInfo.partNumber;
    info.baseName = mpInfo.baseName;
    info.groupKey = mpInfo.groupKey;

    if (info.type == ArchiveType::UNKNOWN && mpInfo.type != ArchiveType::UNKNOWN) {
        info.type = mpInfo.type;
    }

    return info;
}

bool ArchiveDetector::isArchive(const std::string& filePath) {
    auto info = detect(filePath);
    return info.type != ArchiveType::UNKNOWN;
}

bool ArchiveDetector::isMultipart(const std::string& filename) {
    auto info = parseMultipart(filename);
    return info.partNumber >= 0;
}

ArchiveDetector::MultipartInfo ArchiveDetector::parseMultipart(const std::string& filename) {
    MultipartInfo result;
    std::string lower = filename;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // Pattern 1: name.partNN.rar or name.partNN.rar
    {
        std::regex re(R"(^(.+?)\.part(\d+)\.rar$)", std::regex::icase);
        std::smatch match;
        if (std::regex_match(filename, match, re)) {
            result.baseName = match[1].str();
            result.partNumber = std::stoi(match[2].str());
            result.type = ArchiveType::RAR;
            result.groupKey = result.baseName + ":rar";
            std::transform(result.groupKey.begin(), result.groupKey.end(),
                           result.groupKey.begin(), ::tolower);
            return result;
        }
    }

    // Pattern 2: name.7z.NNN
    {
        std::regex re(R"(^(.+?)\.7z\.(\d{3,})$)", std::regex::icase);
        std::smatch match;
        if (std::regex_match(filename, match, re)) {
            result.baseName = match[1].str();
            result.partNumber = std::stoi(match[2].str());
            result.type = ArchiveType::SEVENZIP;
            result.groupKey = result.baseName + ":7z";
            std::transform(result.groupKey.begin(), result.groupKey.end(),
                           result.groupKey.begin(), ::tolower);
            return result;
        }
    }

    // Pattern 3: name.zip, name.z01, name.z02, ...
    {
        std::regex reZip(R"(^(.+?)\.zip$)", std::regex::icase);
        std::regex reZpart(R"(^(.+?)\.z(\d{2,})$)", std::regex::icase);
        std::smatch match;

        if (std::regex_match(filename, match, reZpart)) {
            result.baseName = match[1].str();
            result.partNumber = std::stoi(match[2].str());
            result.type = ArchiveType::ZIP;
            result.groupKey = result.baseName + ":zip";
            std::transform(result.groupKey.begin(), result.groupKey.end(),
                           result.groupKey.begin(), ::tolower);
            return result;
        }
    }

    // Pattern 4: name.rar, name.r00, name.r01, ...
    {
        std::regex reRold(R"(^(.+?)\.r(\d{2,})$)", std::regex::icase);
        std::smatch match;

        if (std::regex_match(filename, match, reRold)) {
            result.baseName = match[1].str();
            result.partNumber = std::stoi(match[2].str()) + 1;  // r00 = part 1 (after .rar = part 0)
            result.type = ArchiveType::RAR;
            result.groupKey = result.baseName + ":rar";
            std::transform(result.groupKey.begin(), result.groupKey.end(),
                           result.groupKey.begin(), ::tolower);
            return result;
        }
    }

    // Pattern 5: name.zip.NNN
    {
        std::regex re(R"(^(.+?)\.zip\.(\d{3,})$)", std::regex::icase);
        std::smatch match;
        if (std::regex_match(filename, match, re)) {
            result.baseName = match[1].str();
            result.partNumber = std::stoi(match[2].str());
            result.type = ArchiveType::ZIP;
            result.groupKey = result.baseName + ":zip";
            std::transform(result.groupKey.begin(), result.groupKey.end(),
                           result.groupKey.begin(), ::tolower);
            return result;
        }
    }

    // Not multipart
    result.baseName = Paths::stem(filename);
    result.partNumber = -1;
    return result;
}

std::vector<std::string> ArchiveDetector::supportedExtensions() {
    return {
        ".zip", ".7z", ".rar", ".tar", ".gz", ".gzip", ".bz2", ".bzip2",
        ".xz", ".zst", ".zstd", ".tgz", ".tbz2", ".txz", ".iso",
        ".z01", ".z02", ".r00", ".r01", ".001", ".002"
    };
}

} // namespace ps5dm
