#pragma once
// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - UI Components
// ═══════════════════════════════════════════════════════════════════════════════
#include "common/types.h"
#include "ui/theme/theme.h"
#include <string>
#include <vector>
#include <functional>

namespace ps5dm {

// ─── Progress Bar ───────────────────────────────────────────────────────────

struct ProgressBarStyle {
    int height = 8;
    Color bgColor;
    Color fillColor;
    bool showPercentage = true;
    bool animated = true;
};

class ProgressBar {
public:
    void setProgress(float percentage);
    float progress() const { return progress_; }
    void setStyle(const ProgressBarStyle& style) { style_ = style; }
    void render(int x, int y, int width);
    std::string renderText(int width) const;

private:
    float progress_ = 0.0f;
    float displayProgress_ = 0.0f;  // smoothed
    ProgressBarStyle style_;
};

// ─── Dialog ─────────────────────────────────────────────────────────────────

struct DialogButton {
    std::string label;
    std::function<void()> action;
    bool isDefault = false;
};

class Dialog {
public:
    void setTitle(const std::string& title) { title_ = title; }
    void setMessage(const std::string& message) { message_ = message; }
    void addButton(const std::string& label, std::function<void()> action, bool isDefault = false);
    void show();
    void hide();
    bool isVisible() const { return visible_; }
    void selectNext();
    void selectPrev();
    void confirm();
    std::string render(int width, int height) const;

private:
    std::string title_;
    std::string message_;
    std::vector<DialogButton> buttons_;
    int selectedButton_ = 0;
    bool visible_ = false;
};

// ─── List View ──────────────────────────────────────────────────────────────

class ListView {
public:
    void setItemCount(int count);
    void setVisibleItems(int count) { visibleItems_ = count; }

    void moveUp();
    void moveDown();
    void pageUp();
    void pageDown();
    void goToTop();
    void goToBottom();

    int selectedIndex() const { return selectedIndex_; }
    void setSelectedIndex(int idx);
    int scrollOffset() const { return scrollOffset_; }
    int visibleItems() const { return visibleItems_; }
    int itemCount() const { return itemCount_; }

    bool isVisible(int index) const;

private:
    int itemCount_ = 0;
    int selectedIndex_ = 0;
    int scrollOffset_ = 0;
    int visibleItems_ = 10;

    void ensureVisible();
};

// ─── Button Bar ─────────────────────────────────────────────────────────────

struct ButtonBarItem {
    std::string icon;    // e.g., "✕", "○", "□", "△"
    std::string label;
};

class ButtonBar {
public:
    void setItems(const std::vector<ButtonBarItem>& items) { items_ = items; }
    void clear() { items_.clear(); }
    std::string render(int width) const;

private:
    std::vector<ButtonBarItem> items_;
};

// ─── Text Input ─────────────────────────────────────────────────────────────

class TextInput {
public:
    void setText(const std::string& text) { text_ = text; cursorPos_ = static_cast<int>(text.size()); }
    const std::string& text() const { return text_; }
    void clear() { text_.clear(); cursorPos_ = 0; }

    void insertChar(char c);
    void backspace();
    void deleteChar();
    void moveCursorLeft();
    void moveCursorRight();
    void moveCursorHome();
    void moveCursorEnd();

    std::string render(const std::string& label, int width) const;

private:
    std::string text_;
    int cursorPos_ = 0;
};

// ─── Notification ───────────────────────────────────────────────────────────

struct NotificationItem {
    std::string title;
    std::string message;
    LogLevel severity = LogLevel::INFO;
    SteadyTime showTime;
    float duration = 4.0f;  // seconds

    bool isExpired() const {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            SteadyClock::now() - showTime).count();
        return elapsed > static_cast<long long>(duration * 1000.0f);
    }
};

class NotificationManager {
public:
    void show(const std::string& title, const std::string& message,
              LogLevel severity = LogLevel::INFO);
    void update();
    std::string render(int width) const;
    bool hasActive() const;

private:
    std::vector<NotificationItem> notifications_;
    static constexpr int MAX_VISIBLE = 3;
};

} // namespace ps5dm
