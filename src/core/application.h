#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Application
// ═══════════════════════════════════════════════════════════════════════════════
#include "common/types.h"
#include "platform/ps5_platform.h"
#include <memory>
#include <atomic>
#include <string>

namespace ps5dm {

/// Command-line arguments
struct AppArgs {
    bool debugMode = false;
    LogLevel logLevel = LogLevel::INFO;
    bool testNetwork = false;
    bool testArchive = false;
    std::string configPath;
};

/// Main application class
class Application {
public:
    static Application& instance();

    /// Parse command-line arguments
    AppArgs parseArgs(int argc, char* argv[]);

    /// Initialize all subsystems
    Result<void> init(const AppArgs& args);

    /// Run the main loop
    int run();

    /// Request shutdown
    void requestShutdown();

    /// Clean shutdown
    void shutdown();

    /// Get platform
    IPlatform* platform() { return platform_.get(); }

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

private:
    Application() = default;

    void mainLoop();
    void checkStorageSpace();
    void checkNetworkStatus();

    std::unique_ptr<IPlatform> platform_;
    std::atomic<bool> running_{false};
    AppArgs args_;
    bool initialized_ = false;
};

} // namespace ps5dm
