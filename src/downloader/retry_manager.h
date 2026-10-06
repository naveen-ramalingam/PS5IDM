#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Retry Manager
// ═══════════════════════════════════════════════════════════════════════════════
#include "common/types.h"
#include <chrono>

namespace ps5dm {

/// Manages retry logic with exponential backoff
class RetryManager {
public:
    explicit RetryManager(int maxRetries = 5, int baseDelaySec = 2);

    /// Check if we should retry
    bool shouldRetry(int currentAttempt, int httpStatus = 0) const;

    /// Get delay before next retry (exponential backoff)
    Seconds getDelay(int attempt) const;

    /// Check if an HTTP status is retryable
    static bool isRetryableHttpStatus(int status);

    /// Check if an error is retryable
    static bool isRetryableError(const Error& error);

    /// Get max retries
    int maxRetries() const { return maxRetries_; }

    void setMaxRetries(int v) { maxRetries_ = v; }
    void setBaseDelay(int seconds) { baseDelaySec_ = seconds; }

private:
    int maxRetries_;
    int baseDelaySec_;
};

} // namespace ps5dm
