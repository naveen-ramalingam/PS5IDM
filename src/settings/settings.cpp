// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Settings Implementation
// ═══════════════════════════════════════════════════════════════════════════════
#include "settings/settings.h"
#include "core/logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace ps5dm {

static const char* TAG = "Settings";

Settings& Settings::instance() {
    static Settings settings;
    return settings;
}

Settings::Settings() {
    initDefaults();
}

void Settings::initDefaults() {
    // Downloads
    setDefault("download.max_active", "3");
    setDefault("download.connections", "8");
    setDefault("download.segment_size", std::to_string(16 * MB));
    setDefault("download.max_retries", "5");
    setDefault("download.retry_delay_base", "2");
    setDefault("download.speed_limit", "0");
    setDefault("download.resume_on_start", "1");

    // Network
    setDefault("network.connect_timeout", "30");
    setDefault("network.transfer_timeout", "300");
    setDefault("network.dns_timeout", "10");
    setDefault("network.user_agent", "PS5DownloadManager/1.0");

    // Storage
    setDefault("storage.download_dir", "/data/downloads");
    setDefault("storage.temp_dir", "/data/downloads/.tmp");
    setDefault("storage.extraction_dir", "/data/downloads");
    setDefault("storage.low_threshold", std::to_string(10 * GB));

    // Archive
    setDefault("archive.recursive_scan", "1");
    setDefault("archive.parallel_jobs", "1");
    setDefault("archive.delete_after_extract", "0");
    setDefault("archive.overwrite_policy", std::to_string(static_cast<int>(OverwritePolicy::ASK)));
    setDefault("archive.verify_before_extract", "1");
    setDefault("archive.auto_extract", "0");

    // UI
    setDefault("ui.font_size", "24");
    setDefault("ui.animations", "1");
    setDefault("ui.confirm_delete", "1");

    // Buffer
    setDefault("buffer.size", std::to_string(1 * MB));

    setDefault("web.enabled", "true");
    setDefault("web.port", "8080");
    setDefault("web.authentication", "true");
    setDefault("web.username", "admin");
    setDefault("web.password", "admin");

    // Debug
    setDefault("debug.log_level", std::to_string(static_cast<int>(LogLevel::INFO)));
}

void Settings::setDefault(const std::string& key, const std::string& value) {
    if (values_.find(key) == values_.end()) {
        values_[key] = value;
    }
}

Result<void> Settings::load(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    filePath_ = path;

    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_WARN(TAG, "Settings file not found, using defaults: " + path);
        return Result<void>::success();
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') continue;

        auto eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;

        std::string key = line.substr(0, eqPos);
        std::string value = line.substr(eqPos + 1);

        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t\r\n") + 1);

        values_[key] = value;
    }

    LOG_INFO(TAG, "Loaded settings from: " + path);
    return Result<void>::success();
}

Result<void> Settings::save() {
    if (filePath_.empty()) {
        return Result<void>::failure(Error::make(1, "No settings file path set"));
    }
    return saveTo(filePath_);
}

Result<void> Settings::saveTo(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        return Result<void>::failure(Error::make(1, "Cannot open settings file for writing: " + path));
    }

    file << "# PS5 Download Manager Settings\n";
    file << "# Generated automatically - edit with caution\n\n";

    // Sort keys for readability
    std::vector<std::string> keys;
    keys.reserve(values_.size());
    for (auto& [k, v] : values_) {
        keys.push_back(k);
    }
    std::sort(keys.begin(), keys.end());

    std::string lastSection;
    for (auto& key : keys) {
        auto dotPos = key.find('.');
        if (dotPos != std::string::npos) {
            std::string section = key.substr(0, dotPos);
            if (section != lastSection) {
                file << "\n# " << section << "\n";
                lastSection = section;
            }
        }
        file << key << "=" << values_[key] << "\n";
    }

    LOG_INFO(TAG, "Saved settings to: " + path);
    return Result<void>::success();
}

// ─── Typed accessors ────────────────────────────────────────────────────────

std::string Settings::getString(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = values_.find(key);
    return (it != values_.end()) ? it->second : "";
}

void Settings::setString(const std::string& key, const std::string& value) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        values_[key] = value;
    }
    notifyChange(key);
}

int Settings::getInt(const std::string& key) const {
    auto s = getString(key);
    try { return s.empty() ? 0 : std::stoi(s); }
    catch (...) { return 0; }
}

void Settings::setInt(const std::string& key, int value) {
    setString(key, std::to_string(value));
}

int64_t Settings::getInt64(const std::string& key) const {
    auto s = getString(key);
    try { return s.empty() ? 0 : std::stoll(s); }
    catch (...) { return 0; }
}

void Settings::setInt64(const std::string& key, int64_t value) {
    setString(key, std::to_string(value));
}

bool Settings::getBool(const std::string& key) const {
    std::string val = getString(key);
    std::transform(val.begin(), val.end(), val.begin(), ::tolower);
    if (val == "true" || val == "1" || val == "yes" || val == "on") {
        return true;
    }
    return false;
}

void Settings::setBool(const std::string& key, bool value) {
    setInt(key, value ? 1 : 0);
}

void Settings::notifyChange(const std::string& key) {
    ChangeCallback cb;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cb = changeCallback_;
    }
    if (cb) cb(key);
}

void Settings::setChangeCallback(ChangeCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    changeCallback_ = std::move(cb);
}

// ─── Download settings ──────────────────────────────────────────────────────
int Settings::maxActiveDownloads() const { return getInt("download.max_active"); }
void Settings::setMaxActiveDownloads(int v) { setInt("download.max_active", std::max(1, std::min(v, 10))); }

int Settings::connectionsPerDownload() const { return getInt("download.connections"); }
void Settings::setConnectionsPerDownload(int v) { setInt("download.connections", std::max(1, std::min(v, 32))); }

FileSize Settings::segmentSize() const { return getInt64("download.segment_size"); }
void Settings::setSegmentSize(FileSize v) { setInt64("download.segment_size", std::max(FileSize(4 * MB), v)); }

int Settings::maxRetries() const { return getInt("download.max_retries"); }
void Settings::setMaxRetries(int v) { setInt("download.max_retries", std::max(0, std::min(v, 100))); }

int Settings::retryDelayBaseSec() const { return getInt("download.retry_delay_base"); }
void Settings::setRetryDelayBaseSec(int v) { setInt("download.retry_delay_base", std::max(1, v)); }

FileSize Settings::downloadSpeedLimit() const { return getInt64("download.speed_limit"); }
void Settings::setDownloadSpeedLimit(FileSize v) { setInt64("download.speed_limit", std::max(FileSize(0), v)); }

bool Settings::resumeDownloadsOnStart() const { return getBool("download.resume_on_start"); }
void Settings::setResumeDownloadsOnStart(bool v) { setBool("download.resume_on_start", v); }

// ─── Network settings ──────────────────────────────────────────────────────
int Settings::connectTimeoutSec() const { return getInt("network.connect_timeout"); }
void Settings::setConnectTimeoutSec(int v) { setInt("network.connect_timeout", std::max(5, v)); }

int Settings::transferTimeoutSec() const { return getInt("network.transfer_timeout"); }
void Settings::setTransferTimeoutSec(int v) { setInt("network.transfer_timeout", std::max(10, v)); }

int Settings::dnsTimeoutSec() const { return getInt("network.dns_timeout"); }
void Settings::setDnsTimeoutSec(int v) { setInt("network.dns_timeout", std::max(3, v)); }

std::string Settings::userAgent() const { return getString("network.user_agent"); }
void Settings::setUserAgent(const std::string& v) { setString("network.user_agent", v); }

std::vector<std::pair<std::string, std::string>> Settings::customHeaders() const {
    // Stored as "key1:value1|key2:value2|..."
    std::vector<std::pair<std::string, std::string>> result;
    auto raw = getString("network.custom_headers");
    if (raw.empty()) return result;

    std::istringstream ss(raw);
    std::string pair;
    while (std::getline(ss, pair, '|')) {
        auto colonPos = pair.find(':');
        if (colonPos != std::string::npos) {
            result.emplace_back(pair.substr(0, colonPos), pair.substr(colonPos + 1));
        }
    }
    return result;
}

void Settings::setCustomHeaders(const std::vector<std::pair<std::string, std::string>>& headers) {
    std::string raw;
    for (size_t i = 0; i < headers.size(); ++i) {
        if (i > 0) raw += '|';
        raw += headers[i].first + ":" + headers[i].second;
    }
    setString("network.custom_headers", raw);
}

// ─── Storage settings ──────────────────────────────────────────────────────
std::string Settings::downloadDirectory() const { return getString("storage.download_dir"); }
void Settings::setDownloadDirectory(const std::string& v) { setString("storage.download_dir", v); }

std::string Settings::tempDirectory() const { return getString("storage.temp_dir"); }
void Settings::setTempDirectory(const std::string& v) { setString("storage.temp_dir", v); }

std::string Settings::extractionDirectory() const { return getString("storage.extraction_dir"); }
void Settings::setExtractionDirectory(const std::string& v) { setString("storage.extraction_dir", v); }

FileSize Settings::lowStorageThreshold() const { return getInt64("storage.low_threshold"); }
void Settings::setLowStorageThreshold(FileSize v) { setInt64("storage.low_threshold", v); }

// ─── Archive settings ──────────────────────────────────────────────────────
bool Settings::recursiveScanning() const { return getBool("archive.recursive_scan"); }
void Settings::setRecursiveScanning(bool v) { setBool("archive.recursive_scan", v); }

int Settings::parallelExtractionJobs() const { return getInt("archive.parallel_jobs"); }
void Settings::setParallelExtractionJobs(int v) { setInt("archive.parallel_jobs", std::max(1, std::min(v, 4))); }

bool Settings::deleteArchiveAfterExtraction() const { return getBool("archive.delete_after_extract"); }
void Settings::setDeleteArchiveAfterExtraction(bool v) { setBool("archive.delete_after_extract", v); }

OverwritePolicy Settings::overwritePolicy() const {
    return static_cast<OverwritePolicy>(getInt("archive.overwrite_policy"));
}
void Settings::setOverwritePolicy(OverwritePolicy v) { setInt("archive.overwrite_policy", static_cast<int>(v)); }

bool Settings::verifyBeforeExtraction() const { return getBool("archive.verify_before_extract"); }
void Settings::setVerifyBeforeExtraction(bool v) { setBool("archive.verify_before_extract", v); }

bool Settings::autoExtract() const { return getBool("archive.auto_extract"); }
void Settings::setAutoExtract(bool v) { setBool("archive.auto_extract", v); }

// ─── UI settings ────────────────────────────────────────────────────────────
int Settings::fontSize() const { return getInt("ui.font_size"); }
void Settings::setFontSize(int v) { setInt("ui.font_size", std::max(16, std::min(v, 48))); }

bool Settings::animationsEnabled() const { return getBool("ui.animations"); }
void Settings::setAnimationsEnabled(bool v) { setBool("ui.animations", v); }

bool Settings::confirmDelete() const { return getBool("ui.confirm_delete"); }
void Settings::setConfirmDelete(bool v) { setBool("ui.confirm_delete", v); }

// ─── Buffer settings ───────────────────────────────────────────────────────
FileSize Settings::bufferSize() const { return getInt64("buffer.size"); }
void Settings::setBufferSize(FileSize v) { setInt64("buffer.size", std::max(FileSize(256 * KB), v)); }

// ─── Web Server settings ──────────────────────────────────────────────────
bool Settings::webServerEnabled() const { return getBool("web.enabled"); }
void Settings::setWebServerEnabled(bool v) { setBool("web.enabled", v); }

int Settings::webServerPort() const { return getInt("web.port"); }
void Settings::setWebServerPort(int v) { setInt("web.port", v); }

bool Settings::webAuthenticationEnabled() const { return getBool("web.authentication"); }
void Settings::setWebAuthenticationEnabled(bool v) { setBool("web.authentication", v); }

std::string Settings::webUsername() const { return getString("web.username"); }
void Settings::setWebUsername(const std::string& v) { setString("web.username", v); }

std::string Settings::webPassword() const { return getString("web.password"); }
void Settings::setWebPassword(const std::string& v) { setString("web.password", v); }

// ─── Debug ──────────────────────────────────────────────────────────────────
LogLevel Settings::logLevel() const { return static_cast<LogLevel>(getInt("debug.log_level")); }
void Settings::setLogLevel(LogLevel v) { setInt("debug.log_level", static_cast<int>(v)); }

} // namespace ps5dm
