#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Theme
// ═══════════════════════════════════════════════════════════════════════════════
#include <cstdint>

namespace ps5dm {

/// Color represented as RGBA
struct Color {
    uint8_t r, g, b, a;
    constexpr Color() : r(0), g(0), b(0), a(255) {}
    constexpr Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
        : r(r), g(g), b(b), a(a) {}
    static constexpr Color hex(uint32_t rgba) {
        return Color(
            static_cast<uint8_t>((rgba >> 24) & 0xFF),
            static_cast<uint8_t>((rgba >> 16) & 0xFF),
            static_cast<uint8_t>((rgba >> 8) & 0xFF),
            static_cast<uint8_t>(rgba & 0xFF)
        );
    }
};

/// PS5-style dark theme
struct Theme {
    // Background
    Color background      = Color(18, 18, 24);
    Color surfacePrimary   = Color(28, 28, 38);
    Color surfaceSecondary = Color(38, 38, 52);
    Color surfaceElevated  = Color(48, 48, 65);

    // Text
    Color textPrimary   = Color(240, 240, 245);
    Color textSecondary = Color(160, 165, 180);
    Color textMuted     = Color(100, 105, 120);
    Color textDisabled  = Color(60, 62, 72);

    // Accent / PS5 blue
    Color accentPrimary   = Color(0, 110, 230);
    Color accentSecondary = Color(0, 85, 190);
    Color accentHighlight = Color(40, 140, 255);

    // Status
    Color success    = Color(46, 204, 113);
    Color warning    = Color(241, 196, 15);
    Color error      = Color(231, 76, 60);
    Color info       = Color(52, 152, 219);

    // Progress bar
    Color progressBg    = Color(40, 40, 55);
    Color progressFill  = Color(0, 110, 230);
    Color progressText  = Color(255, 255, 255);

    // Focus / Selection
    Color focusBorder = Color(0, 130, 255);
    Color selected    = Color(0, 80, 170, 100);
    Color hovered     = Color(60, 60, 80);

    // Separator
    Color separator = Color(50, 52, 65);

    // Button bar
    Color buttonBarBg   = Color(22, 22, 30);
    Color buttonBarText = Color(180, 185, 195);

    // Font sizes
    int fontSizeTitle    = 36;
    int fontSizeHeading  = 28;
    int fontSizeBody     = 22;
    int fontSizeSmall    = 18;
    int fontSizeCaption  = 14;

    // Spacing
    int paddingSmall  = 8;
    int paddingMedium = 16;
    int paddingLarge  = 24;
    int paddingXLarge = 32;

    // Animation
    float animationDuration = 0.2f;  // seconds
    bool animationsEnabled = true;

    /// Get a singleton theme instance
    static Theme& current();
};

} // namespace ps5dm
