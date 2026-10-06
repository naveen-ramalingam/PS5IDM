// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Settings Screen
// ═══════════════════════════════════════════════════════════════════════════════
#include "ui/ui_manager.h"
#include "settings/settings.h"
#include <sstream>

namespace ps5dm {

struct SettingItem {
    std::string label;
    std::string value;
    std::string category;
};

class SettingsScreenImpl : public Screen {
public:
    ScreenId id() const override { return ScreenId::SETTINGS; }
    std::string title() const override { return "Settings"; }

    void onEnter() override { buildItems(); }

    bool handleInput(const InputEvent& event) override {
        if (event.type != InputEvent::BUTTON_DOWN) return false;
        switch (event.button) {
            case ControllerButton::DPAD_UP:   listView_.moveUp(); return true;
            case ControllerButton::DPAD_DOWN: listView_.moveDown(); return true;
            case ControllerButton::DPAD_LEFT:
            case ControllerButton::DPAD_RIGHT:
                adjustSetting(listView_.selectedIndex(),
                              event.button == ControllerButton::DPAD_RIGHT);
                return true;
            case ControllerButton::CROSS:
                adjustSetting(listView_.selectedIndex(), true);
                return true;
            default: break;
        }
        return false;
    }

    void update(float /*deltaTime*/) override {}

    std::string render(int width, int /*height*/) override {
        std::ostringstream oss;
        oss << "  Settings\n\n";

        std::string lastCat;
        for (int i = listView_.scrollOffset();
             i < static_cast<int>(items_.size()) &&
             i < listView_.scrollOffset() + listView_.visibleItems(); ++i) {

            auto& item = items_[static_cast<size_t>(i)];
            if (item.category != lastCat) {
                oss << "\n  ── " << item.category << " ──\n";
                lastCat = item.category;
            }

            bool selected = (i == listView_.selectedIndex());
            if (selected) oss << "▶ "; else oss << "  ";

            oss << std::left;
            int labelWidth = 35;
            std::string label = item.label;
            if (static_cast<int>(label.size()) > labelWidth)
                label = label.substr(0, static_cast<size_t>(labelWidth - 3)) + "...";

            oss.width(labelWidth);
            oss << label;
            oss << "  ◀ " << item.value << " ▶\n";
        }

        oss << "\n  Use ◀ ▶ to change values\n";
        return oss.str();
    }

    std::vector<ButtonBarItem> getButtonBar() const override {
        return {
            {"◀▶", "Adjust"},
            {"✕", "Change"},
            {"L1/R1", "Navigate"}
        };
    }

private:
    void buildItems() {
        items_.clear();
        auto& s = Settings::instance();

        items_.push_back({"Max Active Downloads", std::to_string(s.maxActiveDownloads()), "Downloads"});
        items_.push_back({"Connections per Download", std::to_string(s.connectionsPerDownload()), "Downloads"});
        items_.push_back({"Segment Size", formatSize(s.segmentSize()), "Downloads"});
        items_.push_back({"Max Retries", std::to_string(s.maxRetries()), "Downloads"});
        items_.push_back({"Speed Limit", s.downloadSpeedLimit() > 0 ? formatSpeed(s.downloadSpeedLimit()) : "Unlimited", "Downloads"});
        items_.push_back({"Resume on Start", s.resumeDownloadsOnStart() ? "Yes" : "No", "Downloads"});

        items_.push_back({"Connect Timeout", std::to_string(s.connectTimeoutSec()) + "s", "Network"});
        items_.push_back({"Transfer Timeout", std::to_string(s.transferTimeoutSec()) + "s", "Network"});
        items_.push_back({"User Agent", s.userAgent(), "Network"});

        items_.push_back({"Download Directory", s.downloadDirectory(), "Storage"});
        items_.push_back({"Low Storage Warning", formatSize(s.lowStorageThreshold()), "Storage"});

        items_.push_back({"Recursive Scanning", s.recursiveScanning() ? "Yes" : "No", "Archives"});
        items_.push_back({"Parallel Extraction", std::to_string(s.parallelExtractionJobs()), "Archives"});
        items_.push_back({"Delete After Extract", s.deleteArchiveAfterExtraction() ? "Yes" : "No", "Archives"});
        items_.push_back({"Verify Before Extract", s.verifyBeforeExtraction() ? "Yes" : "No", "Archives"});
        items_.push_back({"Auto Extract", s.autoExtract() ? "Yes" : "No", "Archives"});

        items_.push_back({"Buffer Size", formatSize(s.bufferSize()), "Performance"});
        items_.push_back({"Animations", s.animationsEnabled() ? "Yes" : "No", "UI"});
        items_.push_back({"Confirm Delete", s.confirmDelete() ? "Yes" : "No", "UI"});

        listView_.setItemCount(static_cast<int>(items_.size()));
    }

    void adjustSetting(int index, bool increase) {
        auto& s = Settings::instance();
        if (index < 0 || index >= static_cast<int>(items_.size())) return;

        int delta = increase ? 1 : -1;

        switch (index) {
            case 0: s.setMaxActiveDownloads(s.maxActiveDownloads() + delta); break;
            case 1: {
                int vals[] = {1, 2, 4, 8, 16, 32};
                int cur = s.connectionsPerDownload();
                for (int i = 0; i < 6; ++i) {
                    if (vals[i] == cur) {
                        int ni = std::max(0, std::min(5, i + delta));
                        s.setConnectionsPerDownload(vals[ni]);
                        break;
                    }
                }
                break;
            }
            case 2: {
                FileSize vals[] = {4*MB, 8*MB, 16*MB, 32*MB, 64*MB, 128*MB};
                auto cur = s.segmentSize();
                for (int i = 0; i < 6; ++i) {
                    if (vals[i] == cur) {
                        int ni = std::max(0, std::min(5, i + delta));
                        s.setSegmentSize(vals[ni]);
                        break;
                    }
                }
                break;
            }
            case 3: s.setMaxRetries(s.maxRetries() + delta); break;
            case 5: s.setResumeDownloadsOnStart(!s.resumeDownloadsOnStart()); break;
            case 6: s.setConnectTimeoutSec(s.connectTimeoutSec() + delta * 5); break;
            case 7: s.setTransferTimeoutSec(s.transferTimeoutSec() + delta * 30); break;
            case 11: s.setRecursiveScanning(!s.recursiveScanning()); break;
            case 12: s.setParallelExtractionJobs(s.parallelExtractionJobs() + delta); break;
            case 13: s.setDeleteArchiveAfterExtraction(!s.deleteArchiveAfterExtraction()); break;
            case 14: s.setVerifyBeforeExtraction(!s.verifyBeforeExtraction()); break;
            case 15: s.setAutoExtract(!s.autoExtract()); break;
            case 17: s.setAnimationsEnabled(!s.animationsEnabled()); break;
            case 18: s.setConfirmDelete(!s.confirmDelete()); break;
            default: break;
        }

        s.save();
        buildItems();
    }

    ListView listView_;
    std::vector<SettingItem> items_;
};

std::unique_ptr<Screen> createSettingsScreen() {
    return std::make_unique<SettingsScreenImpl>();
}

} // namespace ps5dm
