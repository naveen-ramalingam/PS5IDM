#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Checksum
// ═══════════════════════════════════════════════════════════════════════════════
#include "common/types.h"
#include <string>
#include <vector>
#include <functional>

namespace ps5dm {

/// Checksum computation
class Checksum {
public:
    /// Compute checksum of a file
    static Result<std::string> computeFile(const std::string& filePath,
                                            ChecksumAlgorithm algorithm,
                                            std::function<bool(FileSize processed,
                                                               FileSize total)> progress = nullptr);

    /// Compute checksum of data in memory
    static std::string computeData(const void* data, size_t size,
                                    ChecksumAlgorithm algorithm);

    /// Verify a file against expected checksum
    static Result<bool> verify(const std::string& filePath,
                                ChecksumAlgorithm algorithm,
                                const std::string& expectedChecksum,
                                std::function<bool(FileSize, FileSize)> progress = nullptr);

    /// Convert binary hash to hex string
    static std::string toHex(const std::vector<uint8_t>& hash);

    /// Compare two checksums (case-insensitive)
    static bool matches(const std::string& a, const std::string& b);
};

} // namespace ps5dm
