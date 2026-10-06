// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Archives Screen
// ═══════════════════════════════════════════════════════════════════════════════
#include "ui/ui_manager.h"
#include "archive/archive_manager.h"
#include "settings/settings.h"
#include <sstream>
#include <iomanip>

namespace ps5dm {

class ArchivesScreenImpl : public Screen {
public:
    ScreenId id() const override { return ScreenId::ARCHIVES; }
    std::string title() const override { return "Archives"; }

    void onEnter() override {
        if (groups_.empty()) {
            ArchiveManager::instance().scanDirectory(
                Settings::instance().downloadDirectory(),
                Settings::instance().recursiveScanning());
        }
        groups_ = ArchiveManager::instance().groups();
        listView_.setItemCount(static_cast<int>(groups_.size()));
    }

    bool handleInput(const InputEvent& event) override {
        if (event.type != InputEvent::BUTTON_DOWN) return false;

        switch (event.button) {
            case ControllerButton::DPAD_UP:    listView_.moveUp(); return true;
            case ControllerButton::DPAD_DOWN:  listView_.moveDown(); return true;
            case ControllerButton::CROSS: {
                int idx = listView_.selectedIndex();
                if (idx >= 0 && idx < static_cast<int>(groups_.size())) {
                    auto& group = groups_[static_cast<size_t>(idx)];
                    if (group->status == ArchiveGroupStatus::READY) {
                        ArchiveManager::instance().extractGroup(group->id);
                    }
                }
                return true;
            }
            case ControllerButton::TRIANGLE: {
                // Rescan
                ArchiveManager::instance().scanDirectory(
                    Settings::instance().downloadDirectory(),
                    Settings::instance().recursiveScanning());
                groups_ = ArchiveManager::instance().groups();
                listView_.setItemCount(static_cast<int>(groups_.size()));
                return true;
            }
            case ControllerButton::SQUARE: {
                // Extract all ready
                ArchiveManager::instance().extractAllReady();
                return true;
            }
            default: break;
        }
        return false;
    }

    void update(float /*deltaTime*/) override {
        groups_ = ArchiveManager::instance().groups();
        listView_.setItemCount(static_cast<int>(groups_.size()));
    }

    std::string render(int width, int /*height*/) override {
        std::ostringstream oss;

        if (groups_.empty()) {
            oss << "\n  No archives found.\n\n";
            oss << "  Press △ to scan for archives.\n";
            return oss.str();
        }

        oss << "  Found " << groups_.size() << " archive groups\n\n";

        for (int i = listView_.scrollOffset();
             i < static_cast<int>(groups_.size()) &&
             i < listView_.scrollOffset() + listView_.visibleItems(); ++i) {

            auto& group = groups_[static_cast<size_t>(i)];
            bool selected = (i == listView_.selectedIndex());

            if (selected) oss << "▶ "; else oss << "  ";

            oss << std::left << std::setw(30) << group->baseName;
            oss << std::setw(8) << archiveTypeToString(group->archiveType);

            if (group->expectedParts > 1) {
                oss << group->foundPartCount() << "/" << group->expectedParts << " parts  ";
            } else {
                oss << "Single  ";
            }

            oss << std::setw(12) << formatSize(group->totalSize);

            // Status with icon
            switch (group->status) {
                case ArchiveGroupStatus::READY:
                    oss << "✓ READY";
                    break;
                case ArchiveGroupStatus::MISSING_PARTS: {
                    auto missing = group->missingParts();
                    oss << "⚠ MISSING " << missing.size() << " parts";
                    break;
                }
                case ArchiveGroupStatus::EXTRACTING:
                    oss << "⟳ EXTRACTING...";
                    break;
                case ArchiveGroupStatus::COMPLETED:
                    oss << "✓ EXTRACTED";
                    break;
                case ArchiveGroupStatus::FAILED:
                    oss << "✖ FAILED";
                    break;
                default:
                    oss << archiveGroupStatusToString(group->status);
                    break;
            }

            oss << "\n";
        }

        // Summary
        int ready = 0, missing = 0, complete = 0;
        for (auto& g : groups_) {
            if (g->status == ArchiveGroupStatus::READY) ready++;
            if (g->status == ArchiveGroupStatus::MISSING_PARTS) missing++;
            if (g->status == ArchiveGroupStatus::COMPLETED) complete++;
        }
        oss << "\n──────────────────────────────────────────────────\n";
        oss << "  Ready: " << ready << "  │  Missing: " << missing
            << "  │  Extracted: " << complete << "\n";

        return oss.str();
    }

    std::vector<ButtonBarItem> getButtonBar() const override {
        return {
            {"✕", "Extract"},
            {"□", "Extract All"},
            {"△", "Rescan"},
            {"L1/R1", "Navigate"}
        };
    }

private:
    ListView listView_;
    std::vector<std::shared_ptr<ArchiveGroup>> groups_;
};

std::unique_ptr<Screen> createArchivesScreen() {
    return std::make_unique<ArchivesScreenImpl>();
}

} // namespace ps5dm
