//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#ifndef CLIENT_DESKTOP_TERMINAL_TERMINAL_WIDGET_H
#define CLIENT_DESKTOP_TERMINAL_TERMINAL_WIDGET_H

#include <QColor>
#include <QFont>
#include <QWidget>

#include <deque>
#include <vector>

#include <vterm.h>

class QMouseEvent;
class QPainter;
class QScrollBar;
class QShowEvent;
class QTimer;
class QWheelEvent;

// Terminal emulator widget. The terminal state machine is driven by libvterm and the visible screen
// is rendered with QPainter. Right after start it shows a local login prompt (user name and password
// entered directly in the terminal); the collected credentials are reported via sig_credentials. Once
// connected, keyboard input is reported via sig_input and the host shell output is fed via
// writeOutput().
class TerminalWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit TerminalWidget(QWidget* parent = nullptr);
    ~TerminalWidget() final;

public slots:
    void writeOutput(const QByteArray& data);
    void onResult(int result_code);

signals:
    void sig_credentials(const QString& user_name, const QString& password);
    void sig_input(const QByteArray& data);
    void sig_resize(int columns, int rows);

protected:
    // QWidget implementation.
    bool event(QEvent* event) final;
    void showEvent(QShowEvent* event) final;
    void paintEvent(QPaintEvent* event) final;
    void keyPressEvent(QKeyEvent* event) final;
    void resizeEvent(QResizeEvent* event) final;
    void focusInEvent(QFocusEvent* event) final;
    void focusOutEvent(QFocusEvent* event) final;
    void wheelEvent(QWheelEvent* event) final;
    void mousePressEvent(QMouseEvent* event) final;
    void mouseReleaseEvent(QMouseEvent* event) final;
    void mouseMoveEvent(QMouseEvent* event) final;
    void mouseDoubleClickEvent(QMouseEvent* event) final;

private:
    enum class Mode
    {
        LOGIN_USER,
        LOGIN_PASSWORD,
        CONNECTED
    };

    // A position in the combined (scrollback + live screen) line stream.
    struct Position
    {
        int line = 0;
        int column = 0;
    };

    void startLoginPrompt();
    void handleLoginKey(QKeyEvent* event);
    void writeLocal(const QString& text);
    void flushInput();
    void drawCell(QPainter& painter, int column, int row, const VTermScreenCell& cell, bool selected) const;
    void updateScrollbar();
    void setScrollOffset(int offset);
    void finishResize();
    bool mouseReportingActive() const;
    void cellAt(const QPoint& position, int* row, int* column) const;
    Position positionAt(const QPoint& position) const;
    bool cellAtLine(int line, int column, VTermScreenCell* cell) const;
    bool isSelected(int line, int column) const;
    QString selectedText() const;
    void selectWordAt(const Position& position);
    void selectLineAt(int line);
    void clearSelection();
    void copySelection();
    void paste();
    void showContextMenu(const QPoint& global_position);
    QColor toColor(const VTermColor& color, const QColor& fallback) const;

    // libvterm screen callbacks.
    static int onDamage(VTermRect rect, void* user);
    static int onMoveCursor(VTermPos pos, VTermPos old_pos, int visible, void* user);
    static int onSetTermProp(VTermProp prop, VTermValue* value, void* user);
    static int onPushLine(int cols, const VTermScreenCell* cells, void* user);
    static int onPopLine(int cols, VTermScreenCell* cells, void* user);
    static void onOutput(const char* bytes, size_t length, void* user);

    VTerm* vterm_ = nullptr;
    VTermScreen* screen_ = nullptr;

    QFont font_;
    int char_width_ = 0;
    int line_height_ = 0;
    int ascent_ = 0;

    int columns_ = 80;
    int rows_ = 24;
    bool resizing_ = false;
    QTimer* resize_timer_ = nullptr;

    int cursor_row_ = 0;
    int cursor_col_ = 0;
    bool cursor_visible_ = true;
    int mouse_mode_ = 0; // VTERM_PROP_MOUSE_NONE
    bool alt_screen_ = false;
    bool suppress_scrollback_ = false;

    Mode mode_ = Mode::LOGIN_USER;
    QString login_user_;
    QString login_password_;

    QByteArray pending_input_;

    std::deque<std::vector<VTermScreenCell>> scrollback_;
    int scroll_offset_ = 0;
    QScrollBar* scrollbar_ = nullptr;

    bool selecting_ = false;
    bool has_selection_ = false;
    Position selection_anchor_;
    Position selection_point_;
    qint64 last_double_click_ms_ = 0;

    QColor default_fg_;
    QColor default_bg_;
    QColor selection_fg_;
    QColor selection_bg_;

    Q_DISABLE_COPY_MOVE(TerminalWidget)
};

#endif // CLIENT_DESKTOP_TERMINAL_TERMINAL_WIDGET_H
