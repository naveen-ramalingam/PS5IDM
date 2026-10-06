// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Download Detail Screen
// ═══════════════════════════════════════════════════════════════════════════════
#include "ui/ui_manager.h"
#include "downloader/download_manager.h"
#include <sstream>
#include <iomanip>

namespace ps5dm {

// Defined in downloads_screen.cpp
extern DownloadId selectedDownloadId_for_detail();

class DownloadDetailScreenImpl : public Screen {
public:
    ScreenId id() const override { return ScreenId::DOWNLOAD_DETAIL; }
    std::string title() const override { return "Download Details"; }

    void onEnter() override {
        // Get the selected download ID from downloads screen
        // We use a simple extern or the static var from DownloadsScreenImpl
    }

    bool handleInput(const InputEvent& event) override {
        if (event.type != InputEvent::BUTTON_DOWN) return false;
        if (event.button == ControllerButton::CIRCLE) {
            UIManager::instance().goBack();
            return true;
        }
        return false;
    }

    void update(float /*deltaTime*/) override {}

    std::string render(int width, int /*height*/) override {
        std::ostringstream oss;

        // Try to get selected download
        auto downloads = DownloadManager::instance().getDownloads();
        if (downloads.empty()) {
            oss << "\n  No download selected.\n";
            return oss.str();
        }

        // For now, show the first active or last download
        std::shared_ptr<DownloadInfo> dl;
        for (auto& d : downloads) {
            if (d->status == DownloadStatus::DOWNLOADING || !dl) {
                dl = d;
            }
        }
        if (!dl) {
            oss << "\n  No download to display.\n";
            return oss.str();
        }

        oss << "\n";
        oss << "  ── Download Details ──\n\n";
        oss << "  Filename:     " << dl->filename << "\n";
        oss << "  URL:          " << (dl->url.size() > 60 ? dl->url.substr(0, 57) + "..." : dl->url) << "\n";
        oss << "  Status:       " << downloadStatusToString(dl->status) << "\n";
        oss << "  Size:         " << formatSize(dl->totalSize) << "\n";
        oss << "  Downloaded:   " << formatSize(dl->downloadedSize.load()) << "\n";

        if (dl->status == DownloadStatus::DOWNLOADING) {
            oss << "  Speed:        " << formatSpeed(dl->speedTracker.currentSpeed()) << "\n";
            oss << "  Avg Speed:    " << formatSpeed(dl->speedTracker.averageSpeed()) << "\n";
            oss << "  Peak Speed:   " << formatSpeed(dl->speedTracker.peakSpeed) << "\n";
            if (dl->totalSize > 0) {
                auto remaining = dl->totalSize - dl->downloadedSize.load();
                oss << "  Remaining:    " << formatSize(remaining) << "\n";
                oss << "  ETA:          " << formatDuration(dl->speedTracker.estimatedTimeRemaining(remaining)) << "\n";
            }
        }

        oss << "  Connections:  " << dl->connectionCount << "\n";
        oss << "  Retries:      " << dl->retryCount << "\n";
        oss << "  Category:     " << downloadCategoryToString(dl->category) << "\n";
        oss << "  Save Path:    " << dl->savePath << "\n";

        if (!dl->etag.empty())
            oss << "  ETag:         " << dl->etag << "\n";
        if (!dl->lastModified.empty())
            oss << "  Modified:     " << dl->lastModified << "\n";
        if (!dl->contentType.empty())
            oss << "  Content-Type: " << dl->contentType << "\n";

        if (dl->lastError) {
            oss << "\n  ── Error ──\n";
            oss << "  " << dl->lastError.message << "\n";
        }

        // Progress bar
        if (dl->totalSize > 0 && dl->status == DownloadStatus::DOWNLOADING) {
            float pct = (static_cast<float>(dl->downloadedSize.load()) /
                        static_cast<float>(dl->totalSize)) * 100.0f;
            oss << "\n  ";
            ProgressBar bar;
            bar.setProgress(pct);
            oss << bar.renderText(width - 6) << "\n";
        }

        return oss.str();
    }

    std::vector<ButtonBarItem> getButtonBar() const override {
        return {
            {"○", "Back"},
            {"□", "Pause/Resume"},
            {"△", "Verify"}
        };
    }

private:
};

std::unique_ptr<Screen> createDownloadDetailScreen() {
    return std::make_unique<DownloadDetailScreenImpl>();
}

} // namespace ps5dm
