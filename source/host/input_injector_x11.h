//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST_INPUT_INJECTOR_X11_H
#define HOST_INPUT_INJECTOR_X11_H

#include "host/input_injector.h"

#include <memory>
#include <set>

#include <X11/X.h>
#include <X11/Xlib.h>

namespace host {

class InputInjectorX11 final : public InputInjector
{
public:
    ~InputInjectorX11();

    static std::unique_ptr<InputInjectorX11> create();

    // InputInjector implementation.
    void setScreenOffset(const base::Point& offset) final;
    void setBlockInput(bool enable) final;
    void injectKeyEvent(const proto::KeyEvent& event) final;
    void injectTextEvent(const proto::TextEvent& event) final;
    void injectMouseEvent(const proto::MouseEvent& event) final;
    void injectTouchEvent(const proto::TouchEvent& event) final;

private:
    InputInjectorX11();
    bool init();

    // Compensates for global button mappings and resets the XTest device mapping.
    void initMouseButtonMap();

    void setLockStates(bool caps, bool num);
    bool isLockKey(int keycode);
    bool isAutoRepeatEnabled();
    void setAutoRepeatEnabled(bool enable);
    void releasePressedKeys();

    // X11 graphics context.
    Display* display_ = nullptr;
    Window root_window_ = BadValue;

    base::Point screen_offset_;

    base::Point last_mouse_pos_;
    bool left_button_pressed_ = false;
    bool right_button_pressed_ = false;
    bool middle_button_pressed_ = false;
    bool back_button_pressed_ = false;
    bool forward_button_pressed_ = false;

    // Number of buttons we support.
    // Left, Right, Middle, VScroll Up/Down, HScroll Left/Right, back, forward.
    static const int kNumPointerButtons = 9;

    int pointer_button_map_[kNumPointerButtons];

    std::set<int> pressed_keys_;

    Q_DISABLE_COPY(InputInjectorX11)
};

} // namespace host

#endif // HOST_INPUT_INJECTOR_X11_H
