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

#ifndef HOST_INPUT_INJECTOR_WAYLAND_H
#define HOST_INPUT_INJECTOR_WAYLAND_H

#include <QPoint>
#include <QPointer>

#include "host/input_injector.h"

class WaylandPortal;

// Injects input on Wayland sessions through the xdg-desktop-portal RemoteDesktop interface of
// |portal| (the same session that drives screen capture). XTest is not available on Wayland, so all
// events go through the portal: keys as evdev keycodes, text as keysyms, the pointer as absolute
// coordinates inside the captured stream.
class InputInjectorWayland final : public InputInjector
{
public:
    explicit InputInjectorWayland(WaylandPortal* portal, QObject* parent = nullptr);
    ~InputInjectorWayland() final;

    // The portal is owned elsewhere (shared with the screen capturer) and must already be started.
    static InputInjectorWayland* create(WaylandPortal* portal, QObject* parent = nullptr);

    // InputInjector implementation.
    void setScreenInfo(const QSize& screen_size, const QPoint& offset) final;
    void setBlockInput(bool enable) final;
    void injectKeyEvent(const proto::input::KeyEvent& event) final;
    void injectTextEvent(const proto::input::TextEvent& event) final;
    void injectMouseEvent(const proto::input::MouseEvent& event) final;
    void injectTouchEvent(const proto::input::TouchEvent& event) final;

private:
    void injectUnicode(uint code_point);

    QPointer<WaylandPortal> portal_;
    QPoint screen_offset_;

    bool left_button_pressed_ = false;
    bool middle_button_pressed_ = false;
    bool right_button_pressed_ = false;
    bool back_button_pressed_ = false;
    bool forward_button_pressed_ = false;

    Q_DISABLE_COPY(InputInjectorWayland)
};

#endif // HOST_INPUT_INJECTOR_WAYLAND_H
