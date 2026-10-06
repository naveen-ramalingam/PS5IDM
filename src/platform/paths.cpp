// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Path Utilities Implementation
// ═══════════════════════════════════════════════════════════════════════════════
#include "platform/paths.h"
#include <algorithm>
#include <sstream>
#include <vector>
#include <sys/stat.h>

namespace ps5dm {

std::string Paths::join(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    if (isAbsolute(b)) return b;

    std::string result = a;
    if (result.back() != '/') result += '/';
    result += b;
    return result;
}

std::string Paths::dirname(const std::string& path) {
    auto pos = path.rfind('/');
    if (pos == std::string::npos) return ".";
    if (pos == 0) return "/";
    return path.substr(0, pos);
}

std::string Paths::basename(const std::string& path) {
    auto pos = path.rfind('/');
    if (pos == std::string::npos) return path;
    return path.substr(pos + 1);
}

std::string Paths::extension(const std::string& path) {
    auto base = basename(path);
    auto pos = base.rfind('.');
    if (pos == std::string::npos || pos == 0) return "";
    return base.substr(pos);  // includes the dot
}

std::string Paths::stem(const std::string& path) {
    auto base = basename(path);
    auto pos = base.rfind('.');
    if (pos == std::string::npos || pos == 0) return base;
    return base.substr(0, pos);
}

std::string Paths::normalize(const std::string& path) {
    if (path.empty()) return ".";

    bool absolute = (path[0] == '/');

    // Split into components
    std::vector<std::string> parts;
    std::istringstream ss(path);
    std::string part;
    while (std::getline(ss, part, '/')) {
        if (part.empty() || part == ".") continue;
        if (part == "..") {
            if (!parts.empty() && parts.back() != "..") {
                parts.pop_back();
            } else if (!absolute) {
                parts.push_back("..");
            }
        } else {
            parts.push_back(part);
        }
    }

    std::string result;
    if (absolute) result = "/";
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) result += '/';
        result += parts[i];
    }

    return result.empty() ? "." : result;
}

bool Paths::isAbsolute(const std::string& path) {
    return !path.empty() && path[0] == '/';
}

bool Paths::hasTraversal(const std::string& path) {
    // Check for directory traversal attempts
    if (path.find("..") != std::string::npos) return true;
    if (path.find("//") != std::string::npos) return true;
    if (isAbsolute(path)) return true;  // Absolute paths in archives are traversal
    return false;
}

std::string Paths::sanitize(const std::string& path) {
    // Remove any traversal attempts - make path safe for extraction
    std::string clean = path;

    // Remove leading /
    while (!clean.empty() && clean[0] == '/') {
        clean.erase(0, 1);
    }

    // Normalize (removes ..)
    clean = normalize(clean);

    // Remove any remaining ..
    while (clean.find("..") != std::string::npos) {
        auto pos = clean.find("..");
        clean.erase(pos, 2);
        // Clean up resulting // or leading /
        while (clean.find("//") != std::string::npos) {
            auto p = clean.find("//");
            clean.erase(p, 1);
        }
        while (!clean.empty() && clean[0] == '/') {
            clean.erase(0, 1);
        }
    }

    return clean.empty() ? "extracted_file" : clean;
}

std::string Paths::makeUnique(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return path;  // Doesn't exist, use as-is
    }

    auto dir = dirname(path);
    auto name = stem(path);
    auto ext = extension(path);

    for (int i = 1; i < 10000; ++i) {
        std::string candidate = join(dir, name + " (" + std::to_string(i) + ")" + ext);
        if (stat(candidate.c_str(), &st) != 0) {
            return candidate;
        }
    }

    return path;  // Give up
}

} // namespace ps5dm
