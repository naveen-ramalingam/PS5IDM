#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - TLS Configuration
// ═══════════════════════════════════════════════════════════════════════════════
#include "common/types.h"
#include <string>

namespace ps5dm {

/// TLS/SSL configuration and initialization
class TLSConfig {
public:
    static Result<void> init();
    static void cleanup();
    static std::string getVersion();
    static bool isAvailable();
};

} // namespace ps5dm
