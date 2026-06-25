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
#include <QSize>

#include "host/input_injector.h"

class WaylandCompositorSource;

// Injects input on Wayland sessions through the compositor |source| (Mutter's RemoteDesktop interface,
// or the xdg-desktop-portal RemoteDesktop session). XTest is not available on Wayland, so all events go
// through it: keys as evdev keycodes, text as keysyms, the pointer as absolute coordinates inside the
// captured stream. The same source also supplies the stream geometry used to rescale coordinates.
class InputInjectorWayland final : public InputInjector
{
public:
    explicit InputInjectorWayland(WaylandCompositorSource* source, QObject* parent = nullptr);
    ~InputInjectorWayland() final;

    // |source| is owned by the screen capturer (shared with it) and must be started.
    static InputInjectorWayland* create(WaylandCompositorSource* source, QObject* parent = nullptr);

    // InputInjector implementation.
    void setScreenInfo(const QSize& screen_size, const QPoint& offset) final;
    void setBlockInput(bool enable) final;
    void injectKeyEvent(const proto::input::KeyEvent& event) final;
    void injectTextEvent(const proto::input::TextEvent& event) final;
    void injectMouseEvent(const proto::input::MouseEvent& event) final;
    void injectTouchEvent(const proto::input::TouchEvent& event) final;

private:
    void injectUnicode(uint code_point);

    WaylandCompositorSource* source_ = nullptr;
    QSize screen_size_;
    QPoint screen_offset_;

    bool left_button_pressed_ = false;
    bool middle_button_pressed_ = false;
    bool right_button_pressed_ = false;
    bool back_button_pressed_ = false;
    bool forward_button_pressed_ = false;

    Q_DISABLE_COPY(InputInjectorWayland)
};

#endif // HOST_INPUT_INJECTOR_WAYLAND_H
