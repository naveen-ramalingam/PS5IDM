// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Hash Worker
// Runs checksum computation in background thread
// ═══════════════════════════════════════════════════════════════════════════════
#include "hashing/checksum.h"
#include "core/event_bus.h"
#include "core/logger.h"
#include <thread>
#include <atomic>
#include <functional>

namespace ps5dm {

/// Background hash worker
class HashWorker {
public:
    using CompleteCallback = std::function<void(const std::string& checksum, Error error)>;
    using ProgressCallback = std::function<void(FileSize processed, FileSize total)>;

    /// Start computing checksum in background
    static void computeAsync(const std::string& filePath,
                              ChecksumAlgorithm algorithm,
                              CompleteCallback onComplete,
                              ProgressCallback onProgress = nullptr) {
        std::thread([filePath, algorithm, onComplete, onProgress] {
            std::atomic<bool> cancelled{false};

            auto result = Checksum::computeFile(filePath, algorithm,
                [&cancelled, &onProgress](FileSize processed, FileSize total) -> bool {
                    if (onProgress) onProgress(processed, total);
                    return !cancelled.load();
                });

            if (result.ok()) {
                if (onComplete) onComplete(result.get(), Error::none());
            } else {
                if (onComplete) onComplete("", result.error);
            }
        }).detach();
    }

    /// Verify file checksum in background
    static void verifyAsync(const std::string& filePath,
                             ChecksumAlgorithm algorithm,
                             const std::string& expectedChecksum,
                             std::function<void(bool match, Error error)> onComplete,
                             ProgressCallback onProgress = nullptr) {
        computeAsync(filePath, algorithm,
            [expectedChecksum, onComplete](const std::string& computed, Error error) {
                if (error) {
                    if (onComplete) onComplete(false, error);
                    return;
                }
                bool match = Checksum::matches(computed, expectedChecksum);
                if (onComplete) onComplete(match, Error::none());
            },
            std::move(onProgress)
        );
    }
};

} // namespace ps5dm
