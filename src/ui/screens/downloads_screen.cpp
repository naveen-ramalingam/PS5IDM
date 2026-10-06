// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Downloads Screen
// ═══════════════════════════════════════════════════════════════════════════════
#include "ui/ui_manager.h"
#include "downloader/download_manager.h"
#include "core/logger.h"
#include <sstream>
#include <iomanip>

namespace ps5dm {

class DownloadsScreenImpl : public Screen {
public:
    ScreenId id() const override { return ScreenId::DOWNLOADS; }
    std::string title() const override { return "Downloads"; }

    void onEnter() override {
        refreshList();
    }

    bool handleInput(const InputEvent& event) override {
        if (event.type != InputEvent::BUTTON_DOWN) return false;

        switch (event.button) {
            case ControllerButton::DPAD_UP:
                listView_.moveUp();
                return true;
            case ControllerButton::DPAD_DOWN:
                listView_.moveDown();
                return true;
            case ControllerButton::CROSS: {
                // Open detail for selected download
                auto downloads = DownloadManager::instance().getDownloads(filter_);
                int idx = listView_.selectedIndex();
                if (idx >= 0 && idx < static_cast<int>(downloads.size())) {
                    selectedDownloadId_ = downloads[static_cast<size_t>(idx)]->id;
                    UIManager::instance().navigateTo(ScreenId::DOWNLOAD_DETAIL);
                }
                return true;
            }
            case ControllerButton::TRIANGLE:
                // Add download
                UIManager::instance().navigateTo(ScreenId::ADD_DOWNLOAD);
                return true;
            case ControllerButton::SQUARE: {
                // Pause/Resume selected
                auto downloads = DownloadManager::instance().getDownloads(filter_);
                int idx = listView_.selectedIndex();
                if (idx >= 0 && idx < static_cast<int>(downloads.size())) {
                    auto& dl = downloads[static_cast<size_t>(idx)];
                    if (dl->status == DownloadStatus::DOWNLOADING) {
                        DownloadManager::instance().pauseDownload(dl->id);
                    } else if (dl->status == DownloadStatus::PAUSED) {
                        DownloadManager::instance().resumeDownload(dl->id);
                    }
                }
                return true;
            }
            case ControllerButton::OPTIONS: {
                // Context menu - show options dialog
                showContextMenu();
                return true;
            }
            default: break;
        }
        return false;
    }

    void update(float /*deltaTime*/) override {
        refreshList();
        DownloadManager::instance().processQueue();
    }

    std::string render(int width, int /*height*/) override {
        std::ostringstream oss;

        auto downloads = DownloadManager::instance().getDownloads(filter_);
        listView_.setItemCount(static_cast<int>(downloads.size()));

        if (downloads.empty()) {
            oss << "\n";
            oss << "  No downloads yet.\n\n";
            oss << "  Press △ to add a download.\n";
            return oss.str();
        }

        // Render download list
        for (int i = listView_.scrollOffset();
             i < static_cast<int>(downloads.size()) &&
             i < listView_.scrollOffset() + listView_.visibleItems(); ++i) {

            auto& dl = downloads[static_cast<size_t>(i)];
            bool selected = (i == listView_.selectedIndex());

            if (selected) oss << "▶ "; else oss << "  ";

            // Filename
            std::string name = dl->filename;
            if (name.size() > 40) name = name.substr(0, 37) + "...";

            oss << std::left << std::setw(42) << name;

            // Status
            switch (dl->status) {
                case DownloadStatus::DOWNLOADING: {
                    float pct = dl->totalSize > 0 ?
                        (static_cast<float>(dl->downloadedSize.load()) /
                         static_cast<float>(dl->totalSize)) * 100.0f : 0.0f;

                    // Progress bar
                    int barWidth = 20;
                    int filled = static_cast<int>((pct / 100.0f) * static_cast<float>(barWidth));
                    std::string bar;
                    for (int b = 0; b < barWidth; ++b) {
                        bar += (b < filled) ? "█" : "░";
                    }

                    oss << bar << " " << std::fixed << std::setprecision(0) << pct << "%";
                    oss << "  " << formatSpeed(dl->speedTracker.currentSpeed());
                    break;
                }
                case DownloadStatus::COMPLETED:
                    oss << "✓ COMPLETE";
                    if (dl->totalSize > 0) oss << "  " << formatSize(dl->totalSize);
                    break;
                case DownloadStatus::PAUSED:
                    oss << "❚❚ PAUSED";
                    if (dl->totalSize > 0) {
                        oss << "  " << formatSize(dl->downloadedSize.load())
                            << " / " << formatSize(dl->totalSize);
                    }
                    break;
                case DownloadStatus::QUEUED:
                    oss << "⏳ QUEUED";
                    break;
                case DownloadStatus::FAILED:
                    oss << "✖ FAILED: " << dl->lastError.message.substr(0, 30);
                    break;
                case DownloadStatus::CONNECTING:
                    oss << "⟳ CONNECTING...";
                    break;
                case DownloadStatus::RETRYING:
                    oss << "⟳ RETRYING (" << dl->retryCount << ")";
                    break;
                default:
                    oss << downloadStatusToString(dl->status);
                    break;
            }

            oss << "\n";

            // Second line for active downloads
            if (selected && dl->status == DownloadStatus::DOWNLOADING && dl->totalSize > 0) {
                oss << "    "
                    << formatSize(dl->downloadedSize.load()) << " / " << formatSize(dl->totalSize)
                    << "    ETA: " << formatDuration(dl->speedTracker.estimatedTimeRemaining(
                           dl->totalSize - dl->downloadedSize.load()))
                    << "    " << dl->connectionCount << " connections\n";
            }
        }

        // Summary line
        oss << "\n──────────────────────────────────────────────────\n";
        int active = DownloadManager::instance().activeCount();
        int queued = DownloadManager::instance().queuedCount();
        oss << "  Active: " << active << "  │  Queued: " << queued
            << "  │  Total: " << downloads.size() << "\n";

        return oss.str();
    }

    std::vector<ButtonBarItem> getButtonBar() const override {
        return {
            {"✕", "Select"},
            {"□", "Pause/Resume"},
            {"△", "Add Download"},
            {"OPTIONS", "More"},
            {"L1/R1", "Navigate"}
        };
    }

    static DownloadId selectedDownloadId_;

private:
    void refreshList() {
        auto downloads = DownloadManager::instance().getDownloads(filter_);
        listView_.setItemCount(static_cast<int>(downloads.size()));
    }

    void showContextMenu() {
        auto downloads = DownloadManager::instance().getDownloads(filter_);
        int idx = listView_.selectedIndex();
        if (idx < 0 || idx >= static_cast<int>(downloads.size())) return;

        auto& dl = downloads[static_cast<size_t>(idx)];

        UIManager::instance().showDialog(
            dl->filename,
            "Choose an action:",
            {
                {"Cancel Download", [id = dl->id]() {
                    DownloadManager::instance().cancelDownload(id);
                }},
                {"Retry", [id = dl->id]() {
                    DownloadManager::instance().retryDownload(id);
                }},
                {"Remove", [id = dl->id]() {
                    DownloadManager::instance().removeDownload(id, false);
                }},
                {"Delete File", [id = dl->id]() {
                    DownloadManager::instance().removeDownload(id, true);
                }, false},
                {"Close", []() {}, true}
            }
        );
    }

    ListView listView_;
    DownloadFilter filter_;
};

DownloadId DownloadsScreenImpl::selectedDownloadId_ = 0;

std::unique_ptr<Screen> createDownloadsScreen() {
    return std::make_unique<DownloadsScreenImpl>();
}

} // namespace ps5dm
