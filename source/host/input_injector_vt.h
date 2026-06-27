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

#ifndef HOST_INPUT_INJECTOR_VT_H
#define HOST_INPUT_INJECTOR_VT_H

#include <QString>

#include <memory>

#include "host/input_injector.h"

class VtMonitors;

// Feeds input into the active virtual terminal. Keystrokes and mouse events are handed to libvterm, which
// produces the right escape sequences. Also owns the terminal-specific gestures: Shift/plain left-drag text
// selection (emitted via sig_terminalClipboard for the host to publish), and paste of the client clipboard
// (middle-click, Shift+Insert, Ctrl+Shift+V). This is the input side of the VT console fallback, paired with
// ScreenCapturerVt.
class InputInjectorVt final : public InputInjector
{
    Q_OBJECT

public:
    explicit InputInjectorVt(std::shared_ptr<VtMonitors> monitors, QObject* parent = nullptr);
    ~InputInjectorVt() final = default;

    // Stores the client's clipboard text; typed into the terminal on a paste gesture.
    void setClipboard(const QString& text) { clipboard_ = text; }

    // InputInjector implementation.
    void setScreenInfo(const QSize& screen_size, const QPoint& offset) final;
    void setBlockInput(bool enable) final;
    void injectKeyEvent(const proto::input::KeyEvent& event) final;
    void injectTextEvent(const proto::input::TextEvent& event) final;
    void injectMouseEvent(const proto::input::MouseEvent& event) final;
    void injectTouchEvent(const proto::input::TouchEvent& event) final;

signals:
    // The text of a finished mouse selection, for the host to publish to the client clipboard.
    void sig_terminalClipboard(const QString& text);

private:
    void paste();

    std::shared_ptr<VtMonitors> monitors_;
    bool shift_ = false;
    bool ctrl_ = false;
    bool alt_ = false;

    // Pixel size of the rendered terminal (from setScreenInfo) and the last button mask, used to map
    // pointer coordinates to cells and to detect button press/release transitions.
    QSize screen_size_;
    quint32 last_buttons_ = 0;

    QString clipboard_; // client clipboard text, pasted on demand

    // Mouse text selection state (cell coordinates of the drag origin).
    bool selecting_ = false;
    int selection_start_col_ = 0;
    int selection_start_row_ = 0;
    bool middle_down_ = false;

    Q_DISABLE_COPY_MOVE(InputInjectorVt)
};

#endif // HOST_INPUT_INJECTOR_VT_H
