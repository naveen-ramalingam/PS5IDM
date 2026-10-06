// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Theme Implementation
// ═══════════════════════════════════════════════════════════════════════════════
#include "ui/theme/theme.h"

namespace ps5dm {

Theme& Theme::current() {
    static Theme theme;
    return theme;
}

} // namespace ps5dm
