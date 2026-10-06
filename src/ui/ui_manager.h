#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Screen Base & UI Manager
// ═══════════════════════════════════════════════════════════════════════════════
#include "common/types.h"
#include "platform/ps5_platform.h"
#include "ui/components/components.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace ps5dm {

/// Screen identifier
enum class ScreenId : int {
    DOWNLOADS = 0,
    ARCHIVES,
    FILES,
    HISTORY,
    SETTINGS,
    DOWNLOAD_DETAIL,
    ADD_DOWNLOAD
};

/// Base class for all screens
class Screen {
public:
    virtual ~Screen() = default;

    virtual ScreenId id() const = 0;
    virtual std::string title() const = 0;

    /// Handle input event, return true if handled
    virtual bool handleInput(const InputEvent& event) = 0;

    /// Update screen state (called each frame)
    virtual void update(float deltaTime) = 0;

    /// Render to text (terminal mode)
    virtual std::string render(int width, int height) = 0;

    /// Called when screen becomes active
    virtual void onEnter() {}

    /// Called when screen becomes inactive
    virtual void onExit() {}

    /// Get button bar items for this screen
    virtual std::vector<ButtonBarItem> getButtonBar() const { return {}; }
};

/// Main UI manager
class UIManager {
public:
    static UIManager& instance();

    /// Initialize UI
    Result<void> init(IPlatform* platform);

    /// Shutdown UI
    void shutdown();

    /// Main update loop iteration
    void update(float deltaTime);

    /// Render current screen
    std::string render();

    /// Handle input
    void handleInput(const InputEvent& event);

    /// Navigate to screen
    void navigateTo(ScreenId screen);

    /// Navigate back
    void goBack();

    /// Get current screen
    ScreenId currentScreen() const { return currentScreen_; }

    /// Show dialog
    void showDialog(const std::string& title, const std::string& message,
                    const std::vector<DialogButton>& buttons);

    /// Show notification
    void showNotification(const std::string& title, const std::string& message,
                          LogLevel severity = LogLevel::INFO);

    /// Is dialog visible?
    bool isDialogVisible() const { return dialog_.isVisible(); }

    /// Get display dimensions
    int displayWidth() const { return displayWidth_; }
    int displayHeight() const { return displayHeight_; }

    UIManager(const UIManager&) = delete;
    UIManager& operator=(const UIManager&) = delete;

private:
    UIManager();

    void initScreens();
    Screen* getScreen(ScreenId id);

    IPlatform* platform_ = nullptr;
    ScreenId currentScreen_ = ScreenId::DOWNLOADS;
    ScreenId previousScreen_ = ScreenId::DOWNLOADS;
    std::vector<std::unique_ptr<Screen>> screens_;
    Dialog dialog_;
    NotificationManager notifications_;
    ButtonBar buttonBar_;
    int displayWidth_ = 120;
    int displayHeight_ = 40;
    bool initialized_ = false;

    // Tab navigation
    static constexpr int TAB_COUNT = 5;
    int currentTab_ = 0;
    static constexpr ScreenId TAB_SCREENS[] = {
        ScreenId::DOWNLOADS, ScreenId::ARCHIVES, ScreenId::FILES,
        ScreenId::HISTORY, ScreenId::SETTINGS
    };
};

} // namespace ps5dm
