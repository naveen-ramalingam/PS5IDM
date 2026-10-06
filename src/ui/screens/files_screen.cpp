// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Files Screen
// ═══════════════════════════════════════════════════════════════════════════════
#include "ui/ui_manager.h"
#include "filesystem/file_manager.h"
#include "platform/paths.h"
#include <sstream>
#include <iomanip>

namespace ps5dm {

class FilesScreenImpl : public Screen {
public:
    ScreenId id() const override { return ScreenId::FILES; }
    std::string title() const override { return "Files"; }

    void onEnter() override {
        auto& fm = FileManager::instance();
        if (fm.entries().empty()) {
            fm.navigateTo(fm.currentPath());
        }
        listView_.setItemCount(static_cast<int>(fm.entries().size()));
    }

    bool handleInput(const InputEvent& event) override {
        auto& fm = FileManager::instance();
        if (event.type != InputEvent::BUTTON_DOWN) return false;

        switch (event.button) {
            case ControllerButton::DPAD_UP:    listView_.moveUp(); return true;
            case ControllerButton::DPAD_DOWN:  listView_.moveDown(); return true;
            case ControllerButton::CROSS: {
                int idx = listView_.selectedIndex();
                auto& entries = fm.entries();
                if (idx >= 0 && idx < static_cast<int>(entries.size())) {
                    if (entries[static_cast<size_t>(idx)].isDirectory) {
                        fm.navigateTo(entries[static_cast<size_t>(idx)].path);
                        listView_.setItemCount(static_cast<int>(fm.entries().size()));
                        listView_.setSelectedIndex(0);
                    }
                }
                return true;
            }
            case ControllerButton::CIRCLE:
                fm.navigateUp();
                listView_.setItemCount(static_cast<int>(fm.entries().size()));
                listView_.setSelectedIndex(0);
                return true;
            case ControllerButton::SQUARE:
                fm.toggleItem(listView_.selectedIndex());
                return true;
            case ControllerButton::OPTIONS:
                showContextMenu();
                return true;
            default: break;
        }
        return false;
    }

    void update(float /*deltaTime*/) override {}

    std::string render(int width, int /*height*/) override {
        auto& fm = FileManager::instance();
        auto& entries = fm.entries();
        std::ostringstream oss;

        oss << "  📁 " << fm.currentPath() << "\n";
        oss << "  " << entries.size() << " items";
        if (fm.selectedCount() > 0) oss << "  (" << fm.selectedCount() << " selected)";
        oss << "\n\n";

        if (entries.empty()) {
            oss << "  (empty directory)\n";
            return oss.str();
        }

        for (int i = listView_.scrollOffset();
             i < static_cast<int>(entries.size()) &&
             i < listView_.scrollOffset() + listView_.visibleItems(); ++i) {

            auto& entry = entries[static_cast<size_t>(i)];
            bool cursor = (i == listView_.selectedIndex());
            bool sel = fm.isSelected(i);

            if (cursor) oss << "▶"; else oss << " ";
            if (sel) oss << "☑ "; else oss << "  ";

            if (entry.isDirectory) {
                oss << "📁 ";
            } else if (entry.isArchive) {
                oss << "📦 ";
            } else {
                oss << "📄 ";
            }

            std::string name = entry.name;
            if (name.size() > 40) name = name.substr(0, 37) + "...";
            oss << std::left << std::setw(42) << name;

            if (!entry.isDirectory) {
                oss << std::right << std::setw(12) << formatSize(entry.size);
            } else {
                oss << std::right << std::setw(12) << "<DIR>";
            }

            oss << "\n";
        }

        return oss.str();
    }

    std::vector<ButtonBarItem> getButtonBar() const override {
        return {
            {"✕", "Open"},
            {"○", "Back"},
            {"□", "Select"},
            {"OPTIONS", "Actions"},
            {"L1/R1", "Navigate"}
        };
    }

private:
    void showContextMenu() {
        auto& fm = FileManager::instance();
        int idx = listView_.selectedIndex();
        auto& entries = fm.entries();
        if (idx < 0 || idx >= static_cast<int>(entries.size())) return;
        auto& entry = entries[static_cast<size_t>(idx)];

        std::vector<DialogButton> buttons;
        buttons.push_back({"Rename", [&fm, idx]() {
            // Would invoke on-screen keyboard
        }});
        buttons.push_back({"Delete", [&fm, idx]() {
            fm.selectItem(idx);
            fm.deleteSelected();
        }});
        buttons.push_back({"New Folder", [&fm]() {
            fm.createFolder("New Folder");
        }});
        if (!entry.isDirectory && entry.isArchive) {
            buttons.push_back({"Extract", [&entry]() {
                // Trigger extraction
            }});
        }
        buttons.push_back({"Close", []() {}, true});

        UIManager::instance().showDialog(entry.name, "Choose action:", buttons);
    }

    ListView listView_;
};

std::unique_ptr<Screen> createFilesScreen() {
    return std::make_unique<FilesScreenImpl>();
}

} // namespace ps5dm
