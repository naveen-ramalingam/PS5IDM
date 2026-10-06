#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Platform Paths
// ═══════════════════════════════════════════════════════════════════════════════
#include <string>

namespace ps5dm {

/// Platform-independent path utilities
class Paths {
public:
    static std::string join(const std::string& a, const std::string& b);
    static std::string dirname(const std::string& path);
    static std::string basename(const std::string& path);
    static std::string extension(const std::string& path);
    static std::string stem(const std::string& path);  // filename without extension
    static std::string normalize(const std::string& path);
    static bool isAbsolute(const std::string& path);
    static bool hasTraversal(const std::string& path);  // detect ../
    static std::string sanitize(const std::string& path);  // remove traversal
    static std::string makeUnique(const std::string& path);  // append (1), (2), etc.
};

} // namespace ps5dm
