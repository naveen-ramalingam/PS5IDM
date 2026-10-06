// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Retry Manager Implementation
// ═══════════════════════════════════════════════════════════════════════════════
#include "downloader/retry_manager.h"
#include <cmath>
#include <algorithm>

namespace ps5dm {

RetryManager::RetryManager(int maxRetries, int baseDelaySec)
    : maxRetries_(maxRetries), baseDelaySec_(baseDelaySec) {}

bool RetryManager::shouldRetry(int currentAttempt, int httpStatus) const {
    if (currentAttempt >= maxRetries_) return false;

    // Don't retry permanent HTTP errors
    if (httpStatus > 0 && !isRetryableHttpStatus(httpStatus)) return false;

    return true;
}

Seconds RetryManager::getDelay(int attempt) const {
    // Exponential backoff: base * 2^attempt, capped at 5 minutes
    int delaySec = baseDelaySec_ * static_cast<int>(std::pow(2, attempt));
    delaySec = std::min(delaySec, 300);  // max 5 minutes
    return Seconds(delaySec);
}

bool RetryManager::isRetryableHttpStatus(int status) {
    switch (status) {
        case 408: return true;  // Request Timeout
        case 429: return true;  // Too Many Requests
        case 500: return true;  // Internal Server Error
        case 502: return true;  // Bad Gateway
        case 503: return true;  // Service Unavailable
        case 504: return true;  // Gateway Timeout
        default:  return false;
    }
}

bool RetryManager::isRetryableError(const Error& error) {
    if (!error) return false;
    if (error.retryable) return true;
    if (error.httpStatus > 0) return isRetryableHttpStatus(error.httpStatus);
    return false;
}

} // namespace ps5dm
