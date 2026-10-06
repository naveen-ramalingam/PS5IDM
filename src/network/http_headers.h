#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - HTTP Headers
// ═══════════════════════════════════════════════════════════════════════════════
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>

namespace ps5dm {

/// Case-insensitive HTTP header storage
class HttpHeaders {
public:
    void set(const std::string& key, const std::string& value);
    std::string get(const std::string& key) const;
    bool has(const std::string& key) const;
    void remove(const std::string& key);
    void clear();

    /// Parse "Header: Value" line
    void parseLine(const std::string& line);

    /// Parse Content-Disposition for filename
    std::string getContentDispositionFilename() const;

    /// Get Content-Length (-1 if not present)
    int64_t getContentLength() const;

    /// Check Accept-Ranges support
    bool supportsRanges() const;

    /// Get ETag
    std::string getETag() const;

    /// Get Last-Modified
    std::string getLastModified() const;

    /// Get Content-Type
    std::string getContentType() const;

    /// Get Retry-After (seconds, -1 if not present)
    int getRetryAfter() const;

    /// Get Location (redirect)
    std::string getLocation() const;

    /// Build curl-compatible header list
    std::vector<std::string> toList() const;

    size_t size() const { return headers_.size(); }

private:
    static std::string toLower(const std::string& s);
    std::unordered_map<std::string, std::string> headers_;
    std::unordered_map<std::string, std::string> originalKeys_;  // preserve case
};

} // namespace ps5dm
