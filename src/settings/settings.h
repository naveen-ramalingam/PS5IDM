#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Settings
// ═══════════════════════════════════════════════════════════════════════════════
#include "common/types.h"
#include <string>
#include <mutex>
#include <unordered_map>
#include <functional>

namespace ps5dm {

/// Application settings with thread-safe access and persistence
class Settings {
public:
    static Settings& instance();

    /// Load settings from file
    Result<void> load(const std::string& path);

    /// Save settings to file
    Result<void> save();

    /// Save to specific path
    Result<void> saveTo(const std::string& path);

    // ─── Download settings ──────────────────────────────────────────────
    int maxActiveDownloads() const;
    void setMaxActiveDownloads(int value);

    int connectionsPerDownload() const;
    void setConnectionsPerDownload(int value);

    FileSize segmentSize() const;
    void setSegmentSize(FileSize value);

    int maxRetries() const;
    void setMaxRetries(int value);

    int retryDelayBaseSec() const;
    void setRetryDelayBaseSec(int value);

    FileSize downloadSpeedLimit() const;  // 0 = unlimited
    void setDownloadSpeedLimit(FileSize bytesPerSec);

    bool resumeDownloadsOnStart() const;
    void setResumeDownloadsOnStart(bool value);

    // ─── Network settings ───────────────────────────────────────────────
    int connectTimeoutSec() const;
    void setConnectTimeoutSec(int value);

    int transferTimeoutSec() const;
    void setTransferTimeoutSec(int value);

    int dnsTimeoutSec() const;
    void setDnsTimeoutSec(int value);

    std::string userAgent() const;
    void setUserAgent(const std::string& value);

    std::vector<std::pair<std::string, std::string>> customHeaders() const;
    void setCustomHeaders(const std::vector<std::pair<std::string, std::string>>& headers);

    // ─── Storage settings ───────────────────────────────────────────────
    std::string downloadDirectory() const;
    void setDownloadDirectory(const std::string& value);

    std::string tempDirectory() const;
    void setTempDirectory(const std::string& value);

    std::string extractionDirectory() const;
    void setExtractionDirectory(const std::string& value);

    FileSize lowStorageThreshold() const;
    void setLowStorageThreshold(FileSize value);

    // ─── Archive settings ───────────────────────────────────────────────
    bool recursiveScanning() const;
    void setRecursiveScanning(bool value);

    int parallelExtractionJobs() const;
    void setParallelExtractionJobs(int value);

    bool deleteArchiveAfterExtraction() const;
    void setDeleteArchiveAfterExtraction(bool value);

    OverwritePolicy overwritePolicy() const;
    void setOverwritePolicy(OverwritePolicy value);

    bool verifyBeforeExtraction() const;
    void setVerifyBeforeExtraction(bool value);

    bool autoExtract() const;
    void setAutoExtract(bool value);

    // ─── UI settings ────────────────────────────────────────────────────
    int fontSize() const;
    void setFontSize(int value);

    bool animationsEnabled() const;
    void setAnimationsEnabled(bool value);

    bool confirmDelete() const;
    void setConfirmDelete(bool value);

    // ─── Buffer settings ────────────────────────────────────────────────
    FileSize bufferSize() const;
    void setBufferSize(FileSize value);

    // ─── Web Server settings ────────────────────────────────────────────
    bool webServerEnabled() const;
    void setWebServerEnabled(bool value);

    int webServerPort() const;
    void setWebServerPort(int value);

    bool webAuthenticationEnabled() const;
    void setWebAuthenticationEnabled(bool value);

    std::string webUsername() const;
    void setWebUsername(const std::string& value);

    std::string webPassword() const;
    void setWebPassword(const std::string& value);

    // ─── Debug ──────────────────────────────────────────────────────────
    LogLevel logLevel() const;
    void setLogLevel(LogLevel value);

    // Change notification
    using ChangeCallback = std::function<void(const std::string& key)>;
    void setChangeCallback(ChangeCallback cb);

    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

private:
    Settings();

    void setDefault(const std::string& key, const std::string& value);
    std::string getString(const std::string& key) const;
    void setString(const std::string& key, const std::string& value);
    int getInt(const std::string& key) const;
    void setInt(const std::string& key, int value);
    int64_t getInt64(const std::string& key) const;
    void setInt64(const std::string& key, int64_t value);
    bool getBool(const std::string& key) const;
    void setBool(const std::string& key, bool value);

    void notifyChange(const std::string& key);
    void initDefaults();

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::string> values_;
    std::string filePath_;
    ChangeCallback changeCallback_;
};

} // namespace ps5dm
