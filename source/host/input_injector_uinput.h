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

#ifndef HOST_INPUT_INJECTOR_UINPUT_H
#define HOST_INPUT_INJECTOR_UINPUT_H

#include <QPoint>
#include <QSize>

#include "host/input_injector.h"

// Injects input through the kernel uinput device (a virtual keyboard and absolute pointer). This works
// below the compositor - the events look like real hardware to GNOME, KDE or X11 alike, and need no
// portal or permission, so it can drive the login screen. Requires write access to /dev/uinput, i.e.
// the agent must run as root.
class InputInjectorUinput final : public InputInjector
{
public:
    explicit InputInjectorUinput(QObject* parent = nullptr);
    ~InputInjectorUinput() final;

    // Returns nullptr if the uinput virtual devices could not be created.
    static InputInjectorUinput* create(QObject* parent = nullptr);

    // InputInjector implementation.
    void setScreenInfo(const QSize& screen_size, const QPoint& offset) final;
    void setBlockInput(bool enable) final;
    void injectKeyEvent(const proto::input::KeyEvent& event) final;
    void injectTextEvent(const proto::input::TextEvent& event) final;
    void injectMouseEvent(const proto::input::MouseEvent& event) final;
    void injectTouchEvent(const proto::input::TouchEvent& event) final;

private:
    bool init();
    void emitEvent(quint16 type, quint16 code, qint32 value);

    int fd_ = -1;
    QSize screen_size_;
    QPoint screen_offset_;

    bool left_button_pressed_ = false;
    bool middle_button_pressed_ = false;
    bool right_button_pressed_ = false;
    bool back_button_pressed_ = false;
    bool forward_button_pressed_ = false;

    Q_DISABLE_COPY(InputInjectorUinput)
};

#endif // HOST_INPUT_INJECTOR_UINPUT_H
