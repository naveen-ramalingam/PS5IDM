// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Mock/Desktop Platform
//
// Allows development and testing on macOS/Linux/Windows without PS5 SDK.
// ═══════════════════════════════════════════════════════════════════════════════
#ifdef MOCK_PLATFORM

#include "platform/ps5_platform.h"
#include "core/logger.h"

#include <thread>
#include <queue>
#include <sys/statvfs.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <atomic>

namespace ps5dm {

static const char* TAG = "MockPlatform";

class MockPlatform : public IPlatform {
public:
    Result<void> init() override {
        LOG_INFO(TAG, "Initializing mock/desktop platform");
        running_ = true;
        return Result<void>::success();
    }

    void shutdown() override {
        LOG_INFO(TAG, "Shutting down mock platform");
        running_ = false;
    }

    DisplayInfo getDisplayInfo() const override {
        DisplayInfo info;
        info.width = 1920;
        info.height = 1080;
        info.refreshRate = 60.0f;
        info.isHDR = false;
        return info;
    }

    Result<void> initGraphics() override {
        LOG_INFO(TAG, "Mock graphics initialized (terminal mode)");
        return Result<void>::success();
    }

    void beginFrame() override {}
    void endFrame() override {}
    void swapBuffers() override {}

    void pollInput() override {
        // Non-blocking terminal input for desktop testing
        char c;
        if (readNonBlocking(c)) {
            InputEvent event;
            event.keyChar = c;
            event.keyCode = static_cast<int>(c);

            // Map keyboard keys to controller buttons for testing
            switch (c) {
                case 'w': case 'W':
                    event.type = InputEvent::BUTTON_DOWN;
                    event.button = ControllerButton::DPAD_UP;
                    break;
                case 's': case 'S':
                    event.type = InputEvent::BUTTON_DOWN;
                    event.button = ControllerButton::DPAD_DOWN;
                    break;
                case 'a': case 'A':
                    event.type = InputEvent::BUTTON_DOWN;
                    event.button = ControllerButton::DPAD_LEFT;
                    break;
                case 'd': case 'D':
                    event.type = InputEvent::BUTTON_DOWN;
                    event.button = ControllerButton::DPAD_RIGHT;
                    break;
                case '\n': case '\r':
                    event.type = InputEvent::BUTTON_DOWN;
                    event.button = ControllerButton::CROSS;
                    break;
                case 27: // ESC
                    event.type = InputEvent::BUTTON_DOWN;
                    event.button = ControllerButton::CIRCLE;
                    break;
                case 'o': case 'O':
                    event.type = InputEvent::BUTTON_DOWN;
                    event.button = ControllerButton::OPTIONS;
                    break;
                case 'q': case 'Q':
                    quitRequested_ = true;
                    return;
                default:
                    event.type = InputEvent::KEY_DOWN;
                    break;
            }
            inputQueue_.push(event);
        }
    }

    ControllerState getControllerState() const override {
        return controllerState_;
    }

    bool hasInputEvent() const override {
        return !inputQueue_.empty();
    }

    InputEvent nextInputEvent() override {
        if (inputQueue_.empty()) return {};
        InputEvent e = inputQueue_.front();
        inputQueue_.pop();
        return e;
    }

    Result<std::string> showKeyboard(const std::string& title,
                                      const std::string& initialText,
                                      int maxLength) override {
        std::cout << "\n┌─── " << title << " ───┐\n";
        std::cout << "│ Current: " << initialText << "\n";
        std::cout << "│ Enter text: ";
        std::cout.flush();

        std::string input;
        std::getline(std::cin, input);

        if (input.empty()) input = initialText;
        if (static_cast<int>(input.size()) > maxLength) {
            input.resize(static_cast<size_t>(maxLength));
        }

        return Result<std::string>::success(input);
    }

    StorageInfo getStorageInfo(const std::string& path) const override {
        StorageInfo info;
        info.mountPoint = path;

        struct statvfs stat;
        if (statvfs(path.c_str(), &stat) == 0) {
            info.totalBytes = static_cast<FileSize>(stat.f_blocks) * static_cast<FileSize>(stat.f_frsize);
            info.freeBytes = static_cast<FileSize>(stat.f_bavail) * static_cast<FileSize>(stat.f_frsize);
            info.usedBytes = info.totalBytes - info.freeBytes;
        }

        return info;
    }

    std::string getDataPath() const override {
        const char* home = getenv("HOME");
        return std::string(home ? home : "/tmp") + "/.ps5dm";
    }

    std::string getTempPath() const override {
        return getDataPath() + "/tmp";
    }

    NetworkStatus getNetworkStatus() const override {
        NetworkStatus status;
        status.connected = true;
        status.interfaceName = "mock0";
        status.ipAddress = "127.0.0.1";
        return status;
    }

    std::string getSystemVersion() const override {
        return "Mock Platform 1.0 (Desktop)";
    }

    bool shouldQuit() const override {
        return quitRequested_.load();
    }

    void showNotification(const std::string& title, const std::string& message) override {
        std::cout << "\n╔═══ NOTIFICATION ═══╗\n";
        std::cout << "║ " << title << "\n";
        std::cout << "║ " << message << "\n";
        std::cout << "╚════════════════════╝\n";
    }

    void sleepMs(int milliseconds) override {
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    }

    uint64_t getTicksMs() const override {
        auto now = std::chrono::steady_clock::now();
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()
        );
    }

private:
    bool readNonBlocking(char& c) {
        struct termios oldt, newt;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~static_cast<unsigned>(ICANON | ECHO);
        newt.c_cc[VMIN] = 0;
        newt.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);

        ssize_t nread = read(STDIN_FILENO, &c, 1);

        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        return nread > 0;
    }

    std::atomic<bool> running_{false};
    std::atomic<bool> quitRequested_{false};
    ControllerState controllerState_;
    std::queue<InputEvent> inputQueue_;
};

std::unique_ptr<IPlatform> createPlatform() {
    return std::make_unique<MockPlatform>();
}

} // namespace ps5dm

#endif // MOCK_PLATFORM
