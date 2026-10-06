// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - HTTP Client Implementation
// ═══════════════════════════════════════════════════════════════════════════════
#include "network/http_client.h"
#include "core/logger.h"
#include "settings/settings.h"

#include <curl/curl.h>
#include <cstring>
#include <sstream>
#include <regex>

namespace ps5dm {

static const char* TAG = "HttpClient";

// ─── Internal implementation ────────────────────────────────────────────────

struct HttpClient::Impl {
    CURL* curl = nullptr;
    std::string userAgent = "PS5DownloadManager/1.0";
    int connectTimeout = 30;
    int transferTimeout = 300;
    int64_t speedLimit = 0;

    Impl() {
        curl = curl_easy_init();
        if (!curl) {
            LOG_FATAL(TAG, "Failed to create CURL handle");
        }
    }

    ~Impl() {
        if (curl) {
            curl_easy_cleanup(curl);
        }
    }

    void applyDefaults(CURL* c) {
        curl_easy_setopt(c, CURLOPT_USERAGENT, userAgent.c_str());
        curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, static_cast<long>(connectTimeout));
        curl_easy_setopt(c, CURLOPT_TIMEOUT, static_cast<long>(transferTimeout));
        curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(c, CURLOPT_MAXREDIRS, 10L);
        curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);

        if (speedLimit > 0) {
            curl_easy_setopt(c, CURLOPT_MAX_RECV_SPEED_LARGE,
                static_cast<curl_off_t>(speedLimit));
        }

        // Enable TCP keepalive
        curl_easy_setopt(c, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(c, CURLOPT_TCP_KEEPIDLE, 60L);
        curl_easy_setopt(c, CURLOPT_TCP_KEEPINTVL, 30L);
    }
};

// ─── libcurl callbacks ──────────────────────────────────────────────────────

struct WriteContext {
    WriteCallback callback;
    std::string* bodyBuffer;  // for non-download (in-memory) requests
};

static size_t curlWriteCallback(void* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* ctx = static_cast<WriteContext*>(userdata);
    size_t totalBytes = size * nmemb;

    if (ctx->callback) {
        return ctx->callback(ptr, totalBytes);
    }
    if (ctx->bodyBuffer) {
        ctx->bodyBuffer->append(static_cast<char*>(ptr), totalBytes);
        return totalBytes;
    }
    return totalBytes;
}

struct HeaderContext {
    HttpHeaders* headers;
    HeaderCallback callback;
};

static size_t curlHeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
    auto* ctx = static_cast<HeaderContext*>(userdata);
    size_t totalBytes = size * nitems;
    std::string line(buffer, totalBytes);

    // Remove trailing newline
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
        line.pop_back();

    if (!line.empty() && ctx->headers) {
        ctx->headers->parseLine(line);
    }
    if (ctx->callback && !line.empty()) {
        ctx->callback(line);
    }

    return totalBytes;
}

struct ProgressContext {
    ProgressCallback callback;
};

static int curlProgressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                                 curl_off_t /*ultotal*/, curl_off_t /*ulnow*/) {
    auto* ctx = static_cast<ProgressContext*>(clientp);
    if (ctx->callback) {
        bool shouldContinue = ctx->callback(static_cast<int64_t>(dlnow),
                                             static_cast<int64_t>(dltotal));
        return shouldContinue ? 0 : 1;  // Return non-zero to abort
    }
    return 0;
}

// ─── Error translation ─────────────────────────────────────────────────────

static Error curlCodeToError(CURLcode code) {
    switch (code) {
        case CURLE_OK:
            return Error::none();
        case CURLE_COULDNT_RESOLVE_HOST:
            return Error::make(static_cast<int>(code), "DNS resolution failed. Check the URL and your network connection.", true);
        case CURLE_COULDNT_CONNECT:
            return Error::make(static_cast<int>(code), "Could not connect to the server. The server may be down.", true);
        case CURLE_OPERATION_TIMEDOUT:
            return Error::make(static_cast<int>(code), "Connection timed out. The server did not respond.", true);
        case CURLE_SSL_CONNECT_ERROR:
        case CURLE_SSL_CERTPROBLEM:
        case CURLE_SSL_CIPHER:
        case CURLE_PEER_FAILED_VERIFICATION:
            return Error::make(static_cast<int>(code), "TLS/SSL connection failed. The connection could not be secured.", false);
        case CURLE_ABORTED_BY_CALLBACK:
            return Error::make(static_cast<int>(code), "Transfer was cancelled.", false);
        case CURLE_WRITE_ERROR:
            return Error::make(static_cast<int>(code), "Failed to write downloaded data to disk.", false);
        case CURLE_RECV_ERROR:
        case CURLE_SEND_ERROR:
            return Error::make(static_cast<int>(code), "Network communication error. The connection was interrupted.", true);
        case CURLE_GOT_NOTHING:
            return Error::make(static_cast<int>(code), "Server returned an empty response.", true);
        case CURLE_PARTIAL_FILE:
            return Error::make(static_cast<int>(code), "Transfer ended prematurely. Only partial data was received.", true);
        case CURLE_RANGE_ERROR:
            return Error::make(static_cast<int>(code), "Server does not support range requests for resume.", false);
        default:
            return Error::make(static_cast<int>(code),
                std::string("Network error: ") + curl_easy_strerror(code), true);
    }
}

// ─── HttpClient implementation ──────────────────────────────────────────────

HttpClient::HttpClient() : impl_(std::make_unique<Impl>()) {}
HttpClient::~HttpClient() = default;

void HttpClient::globalInit() {
    curl_global_init(CURL_GLOBAL_ALL);
    LOG_INFO(TAG, "libcurl initialized: " + std::string(curl_version()));
}

void HttpClient::globalCleanup() {
    curl_global_cleanup();
}

void HttpClient::setUserAgent(const std::string& ua) {
    impl_->userAgent = ua;
}

void HttpClient::setConnectTimeout(int seconds) {
    impl_->connectTimeout = seconds;
}

void HttpClient::setTransferTimeout(int seconds) {
    impl_->transferTimeout = seconds;
}

void HttpClient::setSpeedLimit(int64_t bytesPerSec) {
    impl_->speedLimit = bytesPerSec;
}

Result<HttpResponse> HttpClient::head(const std::string& url) {
    CURL* c = curl_easy_init();
    if (!c) {
        return Result<HttpResponse>::failure(Error::make(1, "Failed to create CURL handle"));
    }

    HttpResponse response;
    HeaderContext headerCtx{&response.headers, nullptr};

    impl_->applyDefaults(c);
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(c, CURLOPT_HEADERFUNCTION, curlHeaderCallback);
    curl_easy_setopt(c, CURLOPT_HEADERDATA, &headerCtx);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 30L);  // HEAD should be fast

    CURLcode res = curl_easy_perform(c);

    if (res != CURLE_OK) {
        curl_easy_cleanup(c);
        return Result<HttpResponse>::failure(curlCodeToError(res));
    }

    long httpCode = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &httpCode);
    response.statusCode = static_cast<int>(httpCode);

    char* effectiveUrl = nullptr;
    curl_easy_getinfo(c, CURLINFO_EFFECTIVE_URL, &effectiveUrl);
    if (effectiveUrl) response.effectiveUrl = effectiveUrl;

    response.contentLength = response.headers.getContentLength();
    response.supportsRanges = response.headers.supportsRanges();
    response.contentType = response.headers.getContentType();

    curl_easy_cleanup(c);

    LOG_DEBUG(TAG, "HEAD " + url + " → " + std::to_string(response.statusCode) +
              ", size=" + std::to_string(response.contentLength) +
              ", ranges=" + (response.supportsRanges ? "yes" : "no"));

    return Result<HttpResponse>::success(std::move(response));
}

Result<HttpResponse> HttpClient::get(const std::string& url) {
    CURL* c = curl_easy_init();
    if (!c) {
        return Result<HttpResponse>::failure(Error::make(1, "Failed to create CURL handle"));
    }

    HttpResponse response;
    std::string body;
    WriteContext writeCtx{nullptr, &body};
    HeaderContext headerCtx{&response.headers, nullptr};

    impl_->applyDefaults(c);
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &writeCtx);
    curl_easy_setopt(c, CURLOPT_HEADERFUNCTION, curlHeaderCallback);
    curl_easy_setopt(c, CURLOPT_HEADERDATA, &headerCtx);

    CURLcode res = curl_easy_perform(c);

    if (res != CURLE_OK) {
        curl_easy_cleanup(c);
        return Result<HttpResponse>::failure(curlCodeToError(res));
    }

    long httpCode = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &httpCode);
    response.statusCode = static_cast<int>(httpCode);
    response.body = std::move(body);

    char* effectiveUrl = nullptr;
    curl_easy_getinfo(c, CURLINFO_EFFECTIVE_URL, &effectiveUrl);
    if (effectiveUrl) response.effectiveUrl = effectiveUrl;

    curl_easy_cleanup(c);
    return Result<HttpResponse>::success(std::move(response));
}

Result<HttpResponse> HttpClient::download(const HttpRequest& request) {
    CURL* c = curl_easy_init();
    if (!c) {
        return Result<HttpResponse>::failure(Error::make(1, "Failed to create CURL handle"));
    }

    HttpResponse response;
    std::string bodyBuffer;
    WriteContext writeCtx{request.writeCallback, request.writeCallback ? nullptr : &bodyBuffer};
    HeaderContext headerCtx{&response.headers, request.headerCallback};
    ProgressContext progressCtx{request.progressCallback};

    impl_->applyDefaults(c);
    curl_easy_setopt(c, CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &writeCtx);
    curl_easy_setopt(c, CURLOPT_HEADERFUNCTION, curlHeaderCallback);
    curl_easy_setopt(c, CURLOPT_HEADERDATA, &headerCtx);

    // Timeouts
    if (request.connectTimeoutSec > 0) {
        curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, static_cast<long>(request.connectTimeoutSec));
    }
    if (request.transferTimeoutSec > 0) {
        curl_easy_setopt(c, CURLOPT_TIMEOUT, static_cast<long>(request.transferTimeoutSec));
    }

    // Progress
    if (request.progressCallback) {
        curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, curlProgressCallback);
        curl_easy_setopt(c, CURLOPT_XFERINFODATA, &progressCtx);
        curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
    }

    // Range
    if (request.rangeStart >= 0) {
        std::string range = std::to_string(request.rangeStart) + "-";
        if (request.rangeEnd >= 0) {
            range += std::to_string(request.rangeEnd);
        }
        curl_easy_setopt(c, CURLOPT_RANGE, range.c_str());
        LOG_DEBUG(TAG, "Range: bytes=" + range);
    }

    // Redirects
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, request.followRedirects ? 1L : 0L);
    curl_easy_setopt(c, CURLOPT_MAXREDIRS, static_cast<long>(request.maxRedirects));

    // SSL
    if (!request.verifySSL) {
        curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    // Custom headers
    struct curl_slist* curlHeaders = nullptr;
    auto headerList = request.headers.toList();
    for (auto& h : headerList) {
        curlHeaders = curl_slist_append(curlHeaders, h.c_str());
    }
    if (curlHeaders) {
        curl_easy_setopt(c, CURLOPT_HTTPHEADER, curlHeaders);
    }

    // Low-speed abort (detect stalled connections)
    curl_easy_setopt(c, CURLOPT_LOW_SPEED_LIMIT, 1L);    // 1 byte/sec
    curl_easy_setopt(c, CURLOPT_LOW_SPEED_TIME, 60L);     // for 60 seconds

    // Perform
    CURLcode res = curl_easy_perform(c);

    if (curlHeaders) curl_slist_free_all(curlHeaders);

    if (res != CURLE_OK) {
        Error err = curlCodeToError(res);
        curl_easy_cleanup(c);
        LOG_WARN(TAG, "Download failed: " + request.url + " - " + err.message);
        return Result<HttpResponse>::failure(std::move(err));
    }

    long httpCode = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &httpCode);
    response.statusCode = static_cast<int>(httpCode);
    response.body = std::move(bodyBuffer);

    char* effectiveUrl = nullptr;
    curl_easy_getinfo(c, CURLINFO_EFFECTIVE_URL, &effectiveUrl);
    if (effectiveUrl) response.effectiveUrl = effectiveUrl;

    double speed = 0.0;
    curl_easy_getinfo(c, CURLINFO_SPEED_DOWNLOAD, &speed);

    curl_easy_cleanup(c);

    response.contentLength = response.headers.getContentLength();
    response.supportsRanges = response.headers.supportsRanges();
    response.contentType = response.headers.getContentType();

    // Check for HTTP errors
    if (response.statusCode >= 400) {
        Error err = Error::http(response.statusCode,
            "HTTP " + std::to_string(response.statusCode));
        response.error = err;
    }

    return Result<HttpResponse>::success(std::move(response));
}

bool HttpClient::isValidUrl(const std::string& url) {
    if (url.empty()) return false;
    if (url.length() < 10) return false;

    // Must start with http:// or https://
    if (url.substr(0, 7) != "http://" && url.substr(0, 8) != "https://") {
        return false;
    }

    // Must have a host after the scheme
    size_t schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos) return false;
    size_t hostStart = schemeEnd + 3;
    if (hostStart >= url.length()) return false;

    // Host can't start with / or be empty
    if (url[hostStart] == '/') return false;

    return true;
}

std::string HttpClient::filenameFromUrl(const std::string& url) {
    // Remove query string and fragment
    std::string clean = url;
    auto queryPos = clean.find('?');
    if (queryPos != std::string::npos) clean = clean.substr(0, queryPos);
    auto fragPos = clean.find('#');
    if (fragPos != std::string::npos) clean = clean.substr(0, fragPos);

    // Find last path segment
    auto lastSlash = clean.rfind('/');
    if (lastSlash == std::string::npos || lastSlash == clean.length() - 1) {
        return "download";
    }

    std::string filename = clean.substr(lastSlash + 1);

    // URL decode basic percent encoding
    std::string decoded;
    decoded.reserve(filename.length());
    for (size_t i = 0; i < filename.length(); ++i) {
        if (filename[i] == '%' && i + 2 < filename.length()) {
            int hex;
            std::istringstream iss(filename.substr(i + 1, 2));
            if (iss >> std::hex >> hex) {
                decoded += static_cast<char>(hex);
                i += 2;
                continue;
            }
        }
        decoded += filename[i];
    }

    return decoded.empty() ? "download" : decoded;
}

} // namespace ps5dm
