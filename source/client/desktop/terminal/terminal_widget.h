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

#include <vterm.h>

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
    void paintEvent(QPaintEvent* event) final;
    void keyPressEvent(QKeyEvent* event) final;
    void resizeEvent(QResizeEvent* event) final;
    void focusInEvent(QFocusEvent* event) final;
    void focusOutEvent(QFocusEvent* event) final;

private:
    enum class Mode
    {
        LOGIN_USER,
        LOGIN_PASSWORD,
        CONNECTED
    };

    void startLoginPrompt();
    void handleLoginKey(QKeyEvent* event);
    void writeLocal(const QString& text);
    void flushInput();
    QColor toColor(const VTermColor& color, const QColor& fallback) const;

    // libvterm screen callbacks.
    static int onDamage(VTermRect rect, void* user);
    static int onMoveCursor(VTermPos pos, VTermPos old_pos, int visible, void* user);
    static int onSetTermProp(VTermProp prop, VTermValue* value, void* user);
    static void onOutput(const char* bytes, size_t length, void* user);

    VTerm* vterm_ = nullptr;
    VTermScreen* screen_ = nullptr;

    QFont font_;
    int char_width_ = 0;
    int line_height_ = 0;
    int ascent_ = 0;

    int columns_ = 80;
    int rows_ = 24;

    int cursor_row_ = 0;
    int cursor_col_ = 0;
    bool cursor_visible_ = true;

    Mode mode_ = Mode::LOGIN_USER;
    QString login_user_;
    QString login_password_;

    QByteArray pending_input_;

    QColor default_fg_;
    QColor default_bg_;

    Q_DISABLE_COPY_MOVE(TerminalWidget)
};

#endif // CLIENT_DESKTOP_TERMINAL_TERMINAL_WIDGET_H
