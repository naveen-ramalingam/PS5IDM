// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - History Screen
// ═══════════════════════════════════════════════════════════════════════════════
#include "ui/ui_manager.h"
#include "database/download_repository.h"
#include <sstream>
#include <iomanip>

namespace ps5dm {

class HistoryScreenImpl : public Screen {
public:
    ScreenId id() const override { return ScreenId::HISTORY; }
    std::string title() const override { return "History"; }

    void onEnter() override {
        history_ = DownloadRepository::instance().loadHistory();
        listView_.setItemCount(static_cast<int>(history_.size()));
    }

    bool handleInput(const InputEvent& event) override {
        if (event.type != InputEvent::BUTTON_DOWN) return false;
        switch (event.button) {
            case ControllerButton::DPAD_UP:   listView_.moveUp(); return true;
            case ControllerButton::DPAD_DOWN: listView_.moveDown(); return true;
            case ControllerButton::TRIANGLE:
                DownloadRepository::instance().clearHistory();
                history_.clear();
                listView_.setItemCount(0);
                return true;
            default: break;
        }
        return false;
    }

    void update(float /*deltaTime*/) override {}

    std::string render(int width, int /*height*/) override {
        std::ostringstream oss;

        if (history_.empty()) {
            oss << "\n  No download history.\n";
            return oss.str();
        }

        oss << "  Download History (" << history_.size() << " entries)\n\n";

        for (int i = listView_.scrollOffset();
             i < static_cast<int>(history_.size()) &&
             i < listView_.scrollOffset() + listView_.visibleItems(); ++i) {

            auto& h = history_[static_cast<size_t>(i)];
            bool selected = (i == listView_.selectedIndex());
            if (selected) oss << "▶ "; else oss << "  ";

            std::string name = h.filename;
            if (name.size() > 35) name = name.substr(0, 32) + "...";
            oss << std::left << std::setw(37) << name;
            oss << std::setw(12) << formatSize(h.fileSize);
            oss << std::setw(14) << formatSpeed(h.avgSpeed);
            oss << h.completedAt.substr(0, 16);
            oss << "\n";
        }

        return oss.str();
    }

    std::vector<ButtonBarItem> getButtonBar() const override {
        return {
            {"✕", "Details"},
            {"△", "Clear History"},
            {"L1/R1", "Navigate"}
        };
    }

private:
    ListView listView_;
    std::vector<DownloadRepository::HistoryEntry> history_;
};

std::unique_ptr<Screen> createHistoryScreen() {
    return std::make_unique<HistoryScreenImpl>();
}

} // namespace ps5dm
