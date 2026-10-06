// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - HTTP Headers Implementation
// ═══════════════════════════════════════════════════════════════════════════════
#include "network/http_headers.h"
#include <sstream>
#include <cctype>

namespace ps5dm {

std::string HttpHeaders::toLower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

void HttpHeaders::set(const std::string& key, const std::string& value) {
    std::string lower = toLower(key);
    headers_[lower] = value;
    originalKeys_[lower] = key;
}

std::string HttpHeaders::get(const std::string& key) const {
    auto it = headers_.find(toLower(key));
    return (it != headers_.end()) ? it->second : "";
}

bool HttpHeaders::has(const std::string& key) const {
    return headers_.find(toLower(key)) != headers_.end();
}

void HttpHeaders::remove(const std::string& key) {
    std::string lower = toLower(key);
    headers_.erase(lower);
    originalKeys_.erase(lower);
}

void HttpHeaders::clear() {
    headers_.clear();
    originalKeys_.clear();
}

void HttpHeaders::parseLine(const std::string& line) {
    auto colonPos = line.find(':');
    if (colonPos == std::string::npos) return;

    std::string key = line.substr(0, colonPos);
    std::string value = line.substr(colonPos + 1);

    // Trim whitespace
    while (!key.empty() && std::isspace(static_cast<unsigned char>(key.back())))
        key.pop_back();
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
        value.erase(0, 1);
    while (!value.empty() && (value.back() == '\r' || value.back() == '\n'))
        value.pop_back();

    set(key, value);
}

std::string HttpHeaders::getContentDispositionFilename() const {
    std::string cd = get("content-disposition");
    if (cd.empty()) return "";

    // Look for filename*= (RFC 5987 extended)
    auto starPos = toLower(cd).find("filename*=");
    if (starPos != std::string::npos) {
        std::string rest = cd.substr(starPos + 10);
        // Format: charset'language'value
        auto quotePos = rest.find('\'');
        if (quotePos != std::string::npos) {
            auto secondQuote = rest.find('\'', quotePos + 1);
            if (secondQuote != std::string::npos) {
                std::string filename = rest.substr(secondQuote + 1);
                // Remove trailing ; or whitespace
                auto endPos = filename.find(';');
                if (endPos != std::string::npos) filename = filename.substr(0, endPos);
                while (!filename.empty() && std::isspace(static_cast<unsigned char>(filename.back())))
                    filename.pop_back();
                // Basic percent-decode
                // TODO: Full percent decoding
                return filename;
            }
        }
    }

    // Look for filename=
    auto fnPos = toLower(cd).find("filename=");
    if (fnPos == std::string::npos) return "";

    std::string rest = cd.substr(fnPos + 9);

    // Remove quotes
    if (!rest.empty() && rest[0] == '"') {
        rest.erase(0, 1);
        auto endQuote = rest.find('"');
        if (endQuote != std::string::npos) {
            return rest.substr(0, endQuote);
        }
    }

    // No quotes
    auto endPos = rest.find(';');
    if (endPos != std::string::npos) rest = rest.substr(0, endPos);
    while (!rest.empty() && std::isspace(static_cast<unsigned char>(rest.back())))
        rest.pop_back();

    return rest;
}

int64_t HttpHeaders::getContentLength() const {
    std::string val = get("content-length");
    if (val.empty()) return -1;
    try { return std::stoll(val); }
    catch (...) { return -1; }
}

bool HttpHeaders::supportsRanges() const {
    std::string val = toLower(get("accept-ranges"));
    return val == "bytes";
}

std::string HttpHeaders::getETag() const {
    return get("etag");
}

std::string HttpHeaders::getLastModified() const {
    return get("last-modified");
}

std::string HttpHeaders::getContentType() const {
    return get("content-type");
}

int HttpHeaders::getRetryAfter() const {
    std::string val = get("retry-after");
    if (val.empty()) return -1;
    try { return std::stoi(val); }
    catch (...) { return -1; }  // Could be an HTTP-date; not supported yet
}

std::string HttpHeaders::getLocation() const {
    return get("location");
}

std::vector<std::string> HttpHeaders::toList() const {
    std::vector<std::string> result;
    result.reserve(headers_.size());
    for (auto& [lower, value] : headers_) {
        auto it = originalKeys_.find(lower);
        std::string key = (it != originalKeys_.end()) ? it->second : lower;
        result.push_back(key + ": " + value);
    }
    return result;
}

} // namespace ps5dm
