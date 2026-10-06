// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Logger Implementation
// ═══════════════════════════════════════════════════════════════════════════════
#include "core/logger.h"
#include <iostream>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <thread>

namespace ps5dm {

const char* logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERR:   return "ERROR";
        case LogLevel::FATAL: return "FATAL";
    }
    return "?????";
}

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

Logger::~Logger() {
    shutdown();
}

void Logger::init(const std::string& logFilePath, LogLevel minLevel) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) return;

    minLevel_ = minLevel;

    if (!logFilePath.empty()) {
        file_.open(logFilePath, std::ios::out | std::ios::app);
        if (!file_.is_open()) {
            std::cerr << "[Logger] Failed to open log file: " << logFilePath << std::endl;
        }
    }
    initialized_ = true;
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }
    initialized_ = false;
}

void Logger::setCallback(LogCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = std::move(cb);
}

void Logger::log(LogLevel level, const std::string& tag, const std::string& message) {
    if (level < minLevel_.load()) return;

    // Build timestamp
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    struct tm tm_buf;
    localtime_r(&time, &tm_buf);

    // Get thread ID
    std::ostringstream tidStream;
    tidStream << std::this_thread::get_id();

    // Format log line
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count()
        << " [" << logLevelToString(level) << "]"
        << " [" << tag << "]"
        << " [T:" << tidStream.str() << "]"
        << " " << message;

    std::string line = oss.str();

    std::lock_guard<std::mutex> lock(mutex_);

    // Console output
    if (level >= LogLevel::WARN) {
        std::cerr << line << std::endl;
    } else {
        std::cout << line << std::endl;
    }

    // File output
    if (file_.is_open()) {
        file_ << line << '\n';
        if (level >= LogLevel::WARN) {
            file_.flush();
        }
    }

    // Callback
    if (callback_) {
        callback_(level, tag, message);
    }
}

} // namespace ps5dm
