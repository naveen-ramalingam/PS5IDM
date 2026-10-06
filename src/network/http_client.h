#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - HTTP Client (libcurl-based)
// ═══════════════════════════════════════════════════════════════════════════════
#include "common/types.h"
#include "network/http_headers.h"
#include <string>
#include <functional>
#include <memory>

namespace ps5dm {

/// HTTP response
struct HttpResponse {
    int statusCode = 0;
    HttpHeaders headers;
    std::string body;  // only for non-download requests
    Error error;
    std::string effectiveUrl;  // after redirects
    std::string contentType;
    int64_t contentLength = -1;
    bool supportsRanges = false;
};

/// HTTP download write callback
using WriteCallback = std::function<size_t(const void* data, size_t size)>;

/// HTTP progress callback
/// Returns false to abort the transfer
using ProgressCallback = std::function<bool(int64_t downloaded, int64_t total)>;

/// HTTP header callback
using HeaderCallback = std::function<void(const std::string& line)>;

/// HTTP request configuration
struct HttpRequest {
    std::string url;
    std::string method = "GET";
    HttpHeaders headers;
    int64_t rangeStart = -1;    // -1 = no range
    int64_t rangeEnd = -1;      // -1 = to end
    int connectTimeoutSec = 30;
    int transferTimeoutSec = 300;
    int dnsTimeoutSec = 10;
    bool followRedirects = true;
    int maxRedirects = 10;
    WriteCallback writeCallback;
    ProgressCallback progressCallback;
    HeaderCallback headerCallback;
    bool verifySSL = true;
};

/// HTTP client wrapping libcurl
class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    /// Initialize global curl (call once at startup)
    static void globalInit();
    static void globalCleanup();

    /// Send HEAD request
    Result<HttpResponse> head(const std::string& url);

    /// Send GET request (full response in memory - for small responses only)
    Result<HttpResponse> get(const std::string& url);

    /// Download with range and write callback
    Result<HttpResponse> download(const HttpRequest& request);

    /// Set default user agent
    void setUserAgent(const std::string& ua);

    /// Set default timeouts
    void setConnectTimeout(int seconds);
    void setTransferTimeout(int seconds);

    /// Set speed limit (bytes/sec, 0 = unlimited)
    void setSpeedLimit(int64_t bytesPerSec);

    /// Validate URL
    static bool isValidUrl(const std::string& url);

    /// Extract filename from URL path
    static std::string filenameFromUrl(const std::string& url);

    // Non-copyable
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ps5dm
