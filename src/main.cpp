// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Entry Point
// ═══════════════════════════════════════════════════════════════════════════════
//
// Usage:
//   ps5-download-manager [options]
//
// Options:
//   --debug              Enable debug mode
//   --log-level=LEVEL    Set log level (trace, debug, info, warn, error)
//   --config=PATH        Use custom config file
//   --test-network       Run network tests
//   --test-archive       Run archive tests
//
// ═══════════════════════════════════════════════════════════════════════════════

#include "core/application.h"
#include <iostream>
#include <csignal>

static void signalHandler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        ps5dm::Application::instance().requestShutdown();
    }
}

int main(int argc, char* argv[]) {
    // Install signal handlers for graceful shutdown
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    auto& app = ps5dm::Application::instance();

    // Parse command-line arguments
    auto args = app.parseArgs(argc, argv);

    // Initialize
    auto result = app.init(args);
    if (!result.ok()) {
        std::cerr << "Initialization failed: " << result.error.message << std::endl;
        return 1;
    }

    // Run
    return app.run();
}
