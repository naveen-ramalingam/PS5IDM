// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Add Download Screen
// ═══════════════════════════════════════════════════════════════════════════════
#include "ui/ui_manager.h"
#include "downloader/download_manager.h"
#include "settings/settings.h"
#include "network/http_client.h"
#include <sstream>

namespace ps5dm {

class AddDownloadScreenImpl : public Screen {
public:
    ScreenId id() const override { return ScreenId::ADD_DOWNLOAD; }
    std::string title() const override { return "Add Download"; }

    void onEnter() override {
        url_.clear();
        filename_.clear();
        savePath_.setText(Settings::instance().downloadDirectory());
        connections_ = Settings::instance().connectionsPerDownload();
        focusField_ = 0;
    }

    bool handleInput(const InputEvent& event) override {
        if (event.type != InputEvent::BUTTON_DOWN) return false;

        switch (event.button) {
            case ControllerButton::DPAD_UP:
                focusField_ = std::max(0, focusField_ - 1);
                return true;
            case ControllerButton::DPAD_DOWN:
                focusField_ = std::min(4, focusField_ + 1);
                return true;
            case ControllerButton::DPAD_LEFT:
                if (focusField_ == 3) connections_ = std::max(1, connections_ / 2);
                return true;
            case ControllerButton::DPAD_RIGHT:
                if (focusField_ == 3) connections_ = std::min(32, connections_ * 2);
                return true;
            case ControllerButton::CROSS: {
                if (focusField_ == 4) {
                    // Start Now
                    startDownload(true);
                    return true;
                }
                // For text fields, would invoke on-screen keyboard
                // For now, accept typed input
                return true;
            }
            case ControllerButton::TRIANGLE: {
                // Add to queue
                startDownload(false);
                return true;
            }
            case ControllerButton::CIRCLE:
                UIManager::instance().goBack();
                return true;
            default:
                // Handle keyboard input for text fields
                if (event.type == InputEvent::KEY_DOWN && event.keyChar != '\0') {
                    switch (focusField_) {
                        case 0: url_.insertChar(event.keyChar); break;
                        case 1: filename_.insertChar(event.keyChar); break;
                        case 2: savePath_.insertChar(event.keyChar); break;
                        default: break;
                    }
                    return true;
                }
                break;
        }
        return false;
    }

    void update(float /*deltaTime*/) override {}

    std::string render(int width, int /*height*/) override {
        std::ostringstream oss;

        oss << "\n  ── Add Download ──\n\n";

        auto renderField = [&](int fieldIdx, const std::string& label, const TextInput& input) {
            bool focused = (fieldIdx == focusField_);
            if (focused) oss << "▶ "; else oss << "  ";
            oss << input.render(label, width - 8) << "\n";
        };

        renderField(0, "URL", url_);
        oss << "\n";
        renderField(1, "Filename", filename_);
        oss << "  (leave empty for auto-detect)\n\n";
        renderField(2, "Save Location", savePath_);
        oss << "\n";

        if (focusField_ == 3) oss << "▶ "; else oss << "  ";
        oss << "Connections:  ◀ " << connections_ << " ▶\n\n";

        if (focusField_ == 4) oss << "▶ "; else oss << "  ";
        oss << "[Start Now]    [Add to Queue]\n\n";

        if (!lastError_.empty()) {
            oss << "  ✖ " << lastError_ << "\n";
        }

        return oss.str();
    }

    std::vector<ButtonBarItem> getButtonBar() const override {
        return {
            {"✕", "Confirm/Edit"},
            {"○", "Cancel"},
            {"△", "Queue"},
            {"◀▶", "Connections"}
        };
    }

private:
    void startDownload(bool immediate) {
        lastError_.clear();

        std::string urlStr = url_.text();
        if (!HttpClient::isValidUrl(urlStr)) {
            lastError_ = "Invalid URL. Must start with http:// or https://";
            return;
        }

        AddDownloadParams params;
        params.url = urlStr;
        params.filename = filename_.text();
        params.savePath = savePath_.text();
        params.connections = connections_;
        params.startImmediately = immediate;

        auto result = DownloadManager::instance().addDownload(params);
        if (result.ok()) {
            if (immediate) {
                DownloadManager::instance().processQueue();
            }
            UIManager::instance().navigateTo(ScreenId::DOWNLOADS);
            UIManager::instance().showNotification("Download Added",
                params.filename.empty() ? "Download queued" : params.filename);
        } else {
            lastError_ = result.error.message;
        }
    }

    TextInput url_;
    TextInput filename_;
    TextInput savePath_;
    int connections_ = 8;
    int focusField_ = 0;
    std::string lastError_;
};

std::unique_ptr<Screen> createAddDownloadScreen() {
    return std::make_unique<AddDownloadScreenImpl>();
}

} // namespace ps5dm
