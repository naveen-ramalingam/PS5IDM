// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - UI Manager Implementation
// ═══════════════════════════════════════════════════════════════════════════════
#include "ui/ui_manager.h"
#include "core/logger.h"
#include "core/event_bus.h"

#include <iostream>
#include <sstream>
#include <algorithm>

namespace ps5dm {

static const char* TAG = "UIManager";

// Forward declarations for screens
class DownloadsScreen;
class ArchivesScreen;
class FilesScreen;
class HistoryScreen;
class SettingsScreen;
class DownloadDetailScreen;
class AddDownloadScreen;

// Screen factory declarations (defined in screen cpp files)
std::unique_ptr<Screen> createDownloadsScreen();
std::unique_ptr<Screen> createArchivesScreen();
std::unique_ptr<Screen> createFilesScreen();
std::unique_ptr<Screen> createHistoryScreen();
std::unique_ptr<Screen> createSettingsScreen();
std::unique_ptr<Screen> createDownloadDetailScreen();
std::unique_ptr<Screen> createAddDownloadScreen();

constexpr ScreenId UIManager::TAB_SCREENS[];

UIManager& UIManager::instance() {
    static UIManager mgr;
    return mgr;
}

UIManager::UIManager() = default;

Result<void> UIManager::init(IPlatform* platform) {
    platform_ = platform;

    auto displayInfo = platform->getDisplayInfo();
    // For terminal mode, use character-based dimensions
    displayWidth_ = 120;
    displayHeight_ = 40;

    initScreens();

    // Subscribe to notification events
    EventBus::instance().subscribe(EventType::UI_NOTIFICATION,
        [this](const Event& event) {
            auto& notif = event.get<NotificationEvent>();
            showNotification(notif.title, notif.message, notif.severity);
        });

    initialized_ = true;
    LOG_INFO(TAG, "UI initialized");
    return Result<void>::success();
}

void UIManager::shutdown() {
    screens_.clear();
    initialized_ = false;
}

void UIManager::initScreens() {
    screens_.push_back(createDownloadsScreen());
    screens_.push_back(createArchivesScreen());
    screens_.push_back(createFilesScreen());
    screens_.push_back(createHistoryScreen());
    screens_.push_back(createSettingsScreen());
    screens_.push_back(createDownloadDetailScreen());
    screens_.push_back(createAddDownloadScreen());

    // Enter first screen
    if (auto* screen = getScreen(currentScreen_)) {
        screen->onEnter();
    }
}

Screen* UIManager::getScreen(ScreenId id) {
    for (auto& screen : screens_) {
        if (screen->id() == id) return screen.get();
    }
    return nullptr;
}

void UIManager::update(float deltaTime) {
    if (!initialized_) return;

    // Process queued events
    EventBus::instance().processQueue();

    // Update notifications
    notifications_.update();

    // Update current screen
    if (auto* screen = getScreen(currentScreen_)) {
        screen->update(deltaTime);
    }
}

std::string UIManager::render() {
    if (!initialized_) return "";

    std::ostringstream oss;

    // ─── Header ─────────────────────────────────────────────────────
    std::string headerLine(static_cast<size_t>(displayWidth_), '-');
    oss << "+" << std::string(static_cast<size_t>(displayWidth_ - 2), '=') << "+\n";
    oss << "║  PS5 DOWNLOAD MANAGER";
    int headerPad = displayWidth_ - 24 - 2;
    if (headerPad > 0) oss << std::string(static_cast<size_t>(headerPad), ' ');
    oss << "|\n";

    // ─── Tab bar ────────────────────────────────────────────────────
    oss << "+" << std::string(static_cast<size_t>(displayWidth_ - 2), '=') << "+\n";
    oss << "|  ";
    const char* tabNames[] = {"Downloads", "Archives", "Files", "History", "Settings"};
    for (int i = 0; i < TAB_COUNT; ++i) {
        if (i == currentTab_) {
            oss << "▶ [" << tabNames[i] << "]  ";
        } else {
            oss << "  " << tabNames[i] << "   ";
        }
    }
    int tabPad = displayWidth_ - 2 - 70;
    if (tabPad > 0) oss << std::string(static_cast<size_t>(tabPad), ' ');
    oss << "|\n";
    oss << "+" << std::string(static_cast<size_t>(displayWidth_ - 2), '-') << "+\n";

    // ─── Screen content ─────────────────────────────────────────────
    if (auto* screen = getScreen(currentScreen_)) {
        std::string content = screen->render(displayWidth_ - 4, displayHeight_ - 10);
        std::istringstream contentStream(content);
        std::string line;
        int lines = 0;
        while (std::getline(contentStream, line) && lines < displayHeight_ - 10) {
            oss << "| ";
            int linePad = displayWidth_ - 4 - static_cast<int>(line.size());
            oss << line;
            if (linePad > 0) oss << std::string(static_cast<size_t>(linePad), ' ');
            oss << " |\n";
            lines++;
        }
        // Fill remaining space
        while (lines < displayHeight_ - 10) {
            oss << "|" << std::string(static_cast<size_t>(displayWidth_ - 2), ' ') << "|\n";
            lines++;
        }
    }

    // ─── Button bar ─────────────────────────────────────────────────
    oss << "+" << std::string(static_cast<size_t>(displayWidth_ - 2), '-') << "+\n";
    oss << "|  ";

    if (auto* screen = getScreen(currentScreen_)) {
        auto barItems = screen->getButtonBar();
        buttonBar_.setItems(barItems);
    }
    std::string barText = buttonBar_.render(displayWidth_ - 4);
    oss << barText;
    int barPad = displayWidth_ - 4 - static_cast<int>(barText.size());
    if (barPad > 0) oss << std::string(static_cast<size_t>(barPad), ' ');
    oss << " |\n";

    oss << "+" << std::string(static_cast<size_t>(displayWidth_ - 2), '=') << "+\n";

    // ─── Notifications overlay ──────────────────────────────────────
    if (notifications_.hasActive()) {
        oss << "\n" << notifications_.render(displayWidth_);
    }

    // ─── Dialog overlay ─────────────────────────────────────────────
    if (dialog_.isVisible()) {
        oss << "\n" << dialog_.render(displayWidth_, displayHeight_);
    }

    return oss.str();
}

void UIManager::handleInput(const InputEvent& event) {
    if (!initialized_) return;

    // Dialog takes priority
    if (dialog_.isVisible()) {
        if (event.type == InputEvent::BUTTON_DOWN) {
            switch (event.button) {
                case ControllerButton::DPAD_LEFT:  dialog_.selectPrev(); break;
                case ControllerButton::DPAD_RIGHT: dialog_.selectNext(); break;
                case ControllerButton::CROSS:      dialog_.confirm(); break;
                case ControllerButton::CIRCLE:     dialog_.hide(); break;
                default: break;
            }
        }
        return;
    }

    // Tab navigation with L1/R1
    if (event.type == InputEvent::BUTTON_DOWN) {
        if (event.button == ControllerButton::L1) {
            currentTab_ = (currentTab_ - 1 + TAB_COUNT) % TAB_COUNT;
            navigateTo(TAB_SCREENS[currentTab_]);
            return;
        }
        if (event.button == ControllerButton::R1) {
            currentTab_ = (currentTab_ + 1) % TAB_COUNT;
            navigateTo(TAB_SCREENS[currentTab_]);
            return;
        }
        if (event.button == ControllerButton::CIRCLE) {
            goBack();
            return;
        }
    }

    // Pass to current screen
    if (auto* screen = getScreen(currentScreen_)) {
        screen->handleInput(event);
    }
}

void UIManager::navigateTo(ScreenId screen) {
    if (auto* current = getScreen(currentScreen_)) {
        current->onExit();
    }

    previousScreen_ = currentScreen_;
    currentScreen_ = screen;

    // Update tab index
    for (int i = 0; i < TAB_COUNT; ++i) {
        if (TAB_SCREENS[i] == screen) {
            currentTab_ = i;
            break;
        }
    }

    if (auto* newScreen = getScreen(screen)) {
        newScreen->onEnter();
    }

    LOG_DEBUG(TAG, "Navigated to screen: " + std::to_string(static_cast<int>(screen)));
}

void UIManager::goBack() {
    navigateTo(previousScreen_);
}

void UIManager::showDialog(const std::string& title, const std::string& message,
                            const std::vector<DialogButton>& buttons) {
    dialog_ = Dialog();
    dialog_.setTitle(title);
    dialog_.setMessage(message);
    for (auto& btn : buttons) {
        dialog_.addButton(btn.label, btn.action, btn.isDefault);
    }
    dialog_.show();
}

void UIManager::showNotification(const std::string& title, const std::string& message,
                                  LogLevel severity) {
    notifications_.show(title, message, severity);
}

} // namespace ps5dm
