// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Checksum Implementation
// ═══════════════════════════════════════════════════════════════════════════════
#include "hashing/checksum.h"
#include "core/logger.h"

#include <openssl/evp.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <sys/stat.h>

namespace ps5dm {

static const char* TAG = "Checksum";

static const EVP_MD* getDigest(ChecksumAlgorithm algo) {
    switch (algo) {
        case ChecksumAlgorithm::MD5:    return EVP_md5();
        case ChecksumAlgorithm::SHA1:   return EVP_sha1();
        case ChecksumAlgorithm::SHA256: return EVP_sha256();
        case ChecksumAlgorithm::SHA512: return EVP_sha512();
        default: return nullptr;
    }
}

Result<std::string> Checksum::computeFile(const std::string& filePath,
                                           ChecksumAlgorithm algorithm,
                                           std::function<bool(FileSize, FileSize)> progress) {
    if (algorithm == ChecksumAlgorithm::NONE) {
        return Result<std::string>::failure(Error::make(1, "No checksum algorithm specified"));
    }

    const EVP_MD* md = getDigest(algorithm);
    if (!md) {
        return Result<std::string>::failure(Error::make(1, "Unsupported algorithm"));
    }

    // Get file size
    struct stat st;
    if (stat(filePath.c_str(), &st) != 0) {
        return Result<std::string>::failure(Error::make(1, "File not found: " + filePath));
    }
    FileSize totalSize = static_cast<FileSize>(st.st_size);

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return Result<std::string>::failure(Error::make(1, "Cannot open file: " + filePath));
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return Result<std::string>::failure(Error::make(1, "Failed to create hash context"));
    }

    EVP_DigestInit_ex(ctx, md, nullptr);

    // Stream the file through the hash
    constexpr size_t BUFFER_SIZE = 1024 * 1024;  // 1 MB
    std::vector<char> buffer(BUFFER_SIZE);
    FileSize processed = 0;

    while (file.read(buffer.data(), static_cast<std::streamsize>(BUFFER_SIZE)) || file.gcount() > 0) {
        auto bytesRead = file.gcount();
        EVP_DigestUpdate(ctx, buffer.data(), static_cast<size_t>(bytesRead));
        processed += bytesRead;

        if (progress) {
            if (!progress(processed, totalSize)) {
                EVP_MD_CTX_free(ctx);
                return Result<std::string>::failure(Error::make(2, "Cancelled"));
            }
        }
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen = 0;
    EVP_DigestFinal_ex(ctx, hash, &hashLen);
    EVP_MD_CTX_free(ctx);

    // Convert to hex
    std::vector<uint8_t> hashVec(hash, hash + hashLen);
    std::string hexStr = toHex(hashVec);

    LOG_DEBUG(TAG, checksumAlgorithmToString(algorithm) + std::string(": ") + hexStr +
              " (" + filePath + ")");

    return Result<std::string>::success(hexStr);
}

std::string Checksum::computeData(const void* data, size_t size,
                                    ChecksumAlgorithm algorithm) {
    const EVP_MD* md = getDigest(algorithm);
    if (!md) return "";

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, md, nullptr);
    EVP_DigestUpdate(ctx, data, size);

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen = 0;
    EVP_DigestFinal_ex(ctx, hash, &hashLen);
    EVP_MD_CTX_free(ctx);

    std::vector<uint8_t> hashVec(hash, hash + hashLen);
    return toHex(hashVec);
}

Result<bool> Checksum::verify(const std::string& filePath,
                               ChecksumAlgorithm algorithm,
                               const std::string& expectedChecksum,
                               std::function<bool(FileSize, FileSize)> progress) {
    auto result = computeFile(filePath, algorithm, std::move(progress));
    if (!result.ok()) return Result<bool>::failure(result.error);

    bool match = matches(result.get(), expectedChecksum);
    return Result<bool>::success(match);
}

std::string Checksum::toHex(const std::vector<uint8_t>& hash) {
    std::ostringstream oss;
    for (auto b : hash) {
        oss << std::hex << std::setfill('0') << std::setw(2)
            << static_cast<unsigned>(b);
    }
    return oss.str();
}

bool Checksum::matches(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    std::string lowerA = a, lowerB = b;
    std::transform(lowerA.begin(), lowerA.end(), lowerA.begin(), ::tolower);
    std::transform(lowerB.begin(), lowerB.end(), lowerB.begin(), ::tolower);
    return lowerA == lowerB;
}

} // namespace ps5dm
