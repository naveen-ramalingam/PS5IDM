#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Logger
// ═══════════════════════════════════════════════════════════════════════════════
#include "common/types.h"
#include <string>
#include <mutex>
#include <fstream>
#include <functional>
#include <atomic>

namespace ps5dm {

/// Thread-safe singleton logger with file and callback output
class Logger {
public:
    static Logger& instance();

    void init(const std::string& logFilePath, LogLevel minLevel = LogLevel::INFO);
    void shutdown();

    void setLevel(LogLevel level) { minLevel_ = level; }
    LogLevel getLevel() const { return minLevel_; }

    /// Set a callback for UI log display
    using LogCallback = std::function<void(LogLevel, const std::string&, const std::string&)>;
    void setCallback(LogCallback cb);

    void log(LogLevel level, const std::string& tag, const std::string& message);

    // Convenience methods
    void trace(const std::string& tag, const std::string& msg) { log(LogLevel::TRACE, tag, msg); }
    void debug(const std::string& tag, const std::string& msg) { log(LogLevel::DEBUG, tag, msg); }
    void info(const std::string& tag, const std::string& msg)  { log(LogLevel::INFO,  tag, msg); }
    void warn(const std::string& tag, const std::string& msg)  { log(LogLevel::WARN,  tag, msg); }
    void error(const std::string& tag, const std::string& msg) { log(LogLevel::ERR,   tag, msg); }
    void fatal(const std::string& tag, const std::string& msg) { log(LogLevel::FATAL, tag, msg); }

    // Disable copy
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger() = default;
    ~Logger();

    std::mutex mutex_;
    std::ofstream file_;
    std::atomic<LogLevel> minLevel_{LogLevel::INFO};
    LogCallback callback_;
    bool initialized_ = false;
};

// Convenience macros
#define LOG_TRACE(tag, msg) ps5dm::Logger::instance().trace(tag, msg)
#define LOG_DEBUG(tag, msg) ps5dm::Logger::instance().debug(tag, msg)
#define LOG_INFO(tag, msg)  ps5dm::Logger::instance().info(tag, msg)
#define LOG_WARN(tag, msg)  ps5dm::Logger::instance().warn(tag, msg)
#define LOG_ERROR(tag, msg) ps5dm::Logger::instance().error(tag, msg)
#define LOG_FATAL(tag, msg) ps5dm::Logger::instance().fatal(tag, msg)

} // namespace ps5dm
