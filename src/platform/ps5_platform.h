#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Platform Abstraction
// ═══════════════════════════════════════════════════════════════════════════════
#include "common/types.h"
#include <string>
#include <memory>
#include <vector>
#include <functional>

namespace ps5dm {

/// Controller button identifiers
enum class ControllerButton : int {
    CROSS = 0,
    CIRCLE,
    SQUARE,
    TRIANGLE,
    DPAD_UP,
    DPAD_DOWN,
    DPAD_LEFT,
    DPAD_RIGHT,
    L1,
    R1,
    L2,
    R2,
    L3,
    R3,
    OPTIONS,
    TOUCHPAD
};

/// Controller input state
struct ControllerState {
    bool buttons[16] = {};
    float leftStickX = 0.0f;
    float leftStickY = 0.0f;
    float rightStickX = 0.0f;
    float rightStickY = 0.0f;
    float leftTrigger = 0.0f;
    float rightTrigger = 0.0f;

    bool isPressed(ControllerButton btn) const {
        return buttons[static_cast<int>(btn)];
    }
};

/// Input event for UI processing
struct InputEvent {
    enum Type { BUTTON_DOWN, BUTTON_UP, STICK_MOVE, KEY_DOWN, KEY_UP };
    Type type;
    ControllerButton button;
    int keyCode = 0;       // For keyboard input (debug/dev)
    char keyChar = '\0';
    float stickX = 0.0f;
    float stickY = 0.0f;
};

/// Display information
struct DisplayInfo {
    int width = 1920;
    int height = 1080;
    float refreshRate = 60.0f;
    bool isHDR = false;
};

/// Storage information
struct StorageInfo {
    FileSize totalBytes = 0;
    FileSize freeBytes = 0;
    FileSize usedBytes = 0;
    std::string mountPoint;
};

/// Network status
struct NetworkStatus {
    bool connected = false;
    std::string interfaceName;
    std::string ipAddress;
    FileSize bandwidthBps = 0;  // estimated, 0 if unknown
};

/// Platform interface - abstracts all PS5/OS-specific functionality
class IPlatform {
public:
    virtual ~IPlatform() = default;

    // ─── Lifecycle ──────────────────────────────────────────────────────
    virtual Result<void> init() = 0;
    virtual void shutdown() = 0;

    // ─── Display ────────────────────────────────────────────────────────
    virtual DisplayInfo getDisplayInfo() const = 0;
    virtual Result<void> initGraphics() = 0;
    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;
    virtual void swapBuffers() = 0;

    // ─── Input ──────────────────────────────────────────────────────────
    virtual void pollInput() = 0;
    virtual ControllerState getControllerState() const = 0;
    virtual bool hasInputEvent() const = 0;
    virtual InputEvent nextInputEvent() = 0;

    // ─── On-screen keyboard ─────────────────────────────────────────────
    virtual Result<std::string> showKeyboard(const std::string& title,
                                              const std::string& initialText,
                                              int maxLength = 256) = 0;

    // ─── Storage ────────────────────────────────────────────────────────
    virtual StorageInfo getStorageInfo(const std::string& path) const = 0;
    virtual std::string getDataPath() const = 0;
    virtual std::string getTempPath() const = 0;

    // ─── Network ────────────────────────────────────────────────────────
    virtual NetworkStatus getNetworkStatus() const = 0;

    // ─── System ─────────────────────────────────────────────────────────
    virtual std::string getSystemVersion() const = 0;
    virtual bool shouldQuit() const = 0;

    // ─── Notifications ──────────────────────────────────────────────────
    virtual void showNotification(const std::string& title, const std::string& message) = 0;

    // ─── Time ───────────────────────────────────────────────────────────
    virtual void sleepMs(int milliseconds) = 0;
    virtual uint64_t getTicksMs() const = 0;
};

/// Create the appropriate platform implementation
std::unique_ptr<IPlatform> createPlatform();

} // namespace ps5dm
