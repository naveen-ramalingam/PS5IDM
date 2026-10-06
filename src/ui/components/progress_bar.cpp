// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - UI Components Implementation
// ═══════════════════════════════════════════════════════════════════════════════

// Progress Bar
#include "ui/components/components.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace ps5dm {

// ─── Progress Bar ───────────────────────────────────────────────────────────

void ProgressBar::setProgress(float percentage) {
    progress_ = std::clamp(percentage, 0.0f, 100.0f);
}

void ProgressBar::render(int x, int y, int width) {
    // Smooth animation
    if (style_.animated) {
        float diff = progress_ - displayProgress_;
        displayProgress_ += diff * 0.1f;
    } else {
        displayProgress_ = progress_;
    }
    // Actual rendering would be done by the platform graphics layer
}

std::string ProgressBar::renderText(int width) const {
    int filled = static_cast<int>((progress_ / 100.0f) * static_cast<float>(width - 2));
    filled = std::clamp(filled, 0, width - 2);

    std::string bar = "[";
    for (int i = 0; i < width - 2; ++i) {
        if (i < filled) bar += "█";
        else bar += "░";
    }
    bar += "]";

    if (style_.showPercentage) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << progress_ << "%";
        bar += " " + oss.str();
    }

    return bar;
}

// ─── Dialog ─────────────────────────────────────────────────────────────────

void Dialog::addButton(const std::string& label, std::function<void()> action, bool isDefault) {
    buttons_.push_back({label, std::move(action), isDefault});
    if (isDefault) selectedButton_ = static_cast<int>(buttons_.size()) - 1;
}

void Dialog::show() { visible_ = true; }
void Dialog::hide() { visible_ = false; }

void Dialog::selectNext() {
    if (!buttons_.empty()) {
        selectedButton_ = (selectedButton_ + 1) % static_cast<int>(buttons_.size());
    }
}

void Dialog::selectPrev() {
    if (!buttons_.empty()) {
        selectedButton_ = (selectedButton_ - 1 + static_cast<int>(buttons_.size())) %
                          static_cast<int>(buttons_.size());
    }
}

void Dialog::confirm() {
    if (selectedButton_ >= 0 && selectedButton_ < static_cast<int>(buttons_.size())) {
        if (buttons_[static_cast<size_t>(selectedButton_)].action) {
            buttons_[static_cast<size_t>(selectedButton_)].action();
        }
    }
    hide();
}

std::string Dialog::render(int width, int /*height*/) const {
    if (!visible_) return "";

    int dialogWidth = std::min(width - 8, 60);
    std::string border;
    for (int i = 0; i < dialogWidth; ++i) border += "="; // ASCII fallback for portability

    std::ostringstream oss;
    oss << "╔" << border << "╗\n";
    oss << "║ " << title_ << std::string(static_cast<size_t>(std::max(0, dialogWidth - 1 - static_cast<int>(title_.size()))), ' ') << "║\n";
    oss << "╠" << border << "╣\n";
    oss << "║ " << message_ << std::string(static_cast<size_t>(std::max(0, dialogWidth - 1 - static_cast<int>(message_.size()))), ' ') << "║\n";
    oss << "╠" << border << "╣\n";

    std::string buttons = "║ ";
    for (int i = 0; i < static_cast<int>(buttons_.size()); ++i) {
        if (i == selectedButton_) buttons += "▶ ";
        buttons += "[" + buttons_[static_cast<size_t>(i)].label + "]  ";
    }
    int pad = std::max(0, dialogWidth - static_cast<int>(buttons.size()) + 2);
    buttons += std::string(static_cast<size_t>(pad), ' ') + "║";
    oss << buttons << "\n";

    oss << "╚" << border << "╝\n";
    return oss.str();
}

// ─── List View ──────────────────────────────────────────────────────────────

void ListView::setItemCount(int count) {
    itemCount_ = count;
    if (selectedIndex_ >= count) selectedIndex_ = std::max(0, count - 1);
    ensureVisible();
}

void ListView::setSelectedIndex(int idx) {
    selectedIndex_ = std::clamp(idx, 0, std::max(0, itemCount_ - 1));
    ensureVisible();
}

void ListView::moveUp() {
    if (selectedIndex_ > 0) {
        selectedIndex_--;
        ensureVisible();
    }
}

void ListView::moveDown() {
    if (selectedIndex_ < itemCount_ - 1) {
        selectedIndex_++;
        ensureVisible();
    }
}

void ListView::pageUp() {
    selectedIndex_ = std::max(0, selectedIndex_ - visibleItems_);
    ensureVisible();
}

void ListView::pageDown() {
    selectedIndex_ = std::min(itemCount_ - 1, selectedIndex_ + visibleItems_);
    ensureVisible();
}

void ListView::goToTop() {
    selectedIndex_ = 0;
    scrollOffset_ = 0;
}

void ListView::goToBottom() {
    selectedIndex_ = std::max(0, itemCount_ - 1);
    ensureVisible();
}

bool ListView::isVisible(int index) const {
    return index >= scrollOffset_ && index < scrollOffset_ + visibleItems_;
}

void ListView::ensureVisible() {
    if (selectedIndex_ < scrollOffset_) {
        scrollOffset_ = selectedIndex_;
    }
    if (selectedIndex_ >= scrollOffset_ + visibleItems_) {
        scrollOffset_ = selectedIndex_ - visibleItems_ + 1;
    }
    scrollOffset_ = std::max(0, scrollOffset_);
}

// ─── Button Bar ─────────────────────────────────────────────────────────────

std::string ButtonBar::render(int /*width*/) const {
    std::ostringstream oss;
    for (size_t i = 0; i < items_.size(); ++i) {
        if (i > 0) oss << "  │  ";
        oss << items_[i].icon << " " << items_[i].label;
    }
    return oss.str();
}

// ─── Text Input ─────────────────────────────────────────────────────────────

void TextInput::insertChar(char c) {
    text_.insert(static_cast<size_t>(cursorPos_), 1, c);
    cursorPos_++;
}

void TextInput::backspace() {
    if (cursorPos_ > 0) {
        text_.erase(static_cast<size_t>(cursorPos_ - 1), 1);
        cursorPos_--;
    }
}

void TextInput::deleteChar() {
    if (cursorPos_ < static_cast<int>(text_.size())) {
        text_.erase(static_cast<size_t>(cursorPos_), 1);
    }
}

void TextInput::moveCursorLeft() {
    if (cursorPos_ > 0) cursorPos_--;
}

void TextInput::moveCursorRight() {
    if (cursorPos_ < static_cast<int>(text_.size())) cursorPos_++;
}

void TextInput::moveCursorHome() { cursorPos_ = 0; }
void TextInput::moveCursorEnd() { cursorPos_ = static_cast<int>(text_.size()); }

std::string TextInput::render(const std::string& label, int width) const {
    std::ostringstream oss;
    oss << label << ": [";
    std::string display = text_;
    int maxLen = width - static_cast<int>(label.size()) - 5;
    if (static_cast<int>(display.size()) > maxLen) {
        display = display.substr(static_cast<size_t>(std::max(0, cursorPos_ - maxLen + 5)));
        if (static_cast<int>(display.size()) > maxLen) display.resize(static_cast<size_t>(maxLen));
    }
    oss << display;
    int pad = maxLen - static_cast<int>(display.size());
    if (pad > 0) oss << std::string(static_cast<size_t>(pad), '_');
    oss << "]";
    return oss.str();
}

// ─── Notification Manager ───────────────────────────────────────────────────

void NotificationManager::show(const std::string& title, const std::string& message,
                                LogLevel severity) {
    NotificationItem item;
    item.title = title;
    item.message = message;
    item.severity = severity;
    item.showTime = SteadyClock::now();
    notifications_.push_back(item);

    // Limit to MAX_VISIBLE
    while (static_cast<int>(notifications_.size()) > MAX_VISIBLE) {
        notifications_.erase(notifications_.begin());
    }
}

void NotificationManager::update() {
    notifications_.erase(
        std::remove_if(notifications_.begin(), notifications_.end(),
            [](const NotificationItem& n) { return n.isExpired(); }),
        notifications_.end()
    );
}

bool NotificationManager::hasActive() const {
    return !notifications_.empty();
}

std::string NotificationManager::render(int /*width*/) const {
    std::ostringstream oss;
    for (auto& n : notifications_) {
        const char* icon = "ℹ";
        if (n.severity == LogLevel::WARN) icon = "⚠";
        if (n.severity >= LogLevel::ERR) icon = "✖";

        oss << "┌────────────────────────────────────┐\n";
        oss << "│ " << icon << " " << n.title << "\n";
        oss << "│ " << n.message << "\n";
        oss << "└────────────────────────────────────┘\n";
    }
    return oss.str();
}

} // namespace ps5dm
