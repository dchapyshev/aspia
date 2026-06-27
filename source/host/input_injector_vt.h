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

#include <memory>

#include "host/input_injector.h"

class VtMonitors;

// Feeds input into the active virtual terminal. Key events (USB HID codes) and mouse events are handed to
// libvterm, which produces the right escape sequences (and emits nothing for mouse unless the application
// enabled mouse reporting). This is the input side of the VT console fallback, paired with ScreenCapturerVt.
class InputInjectorVt final : public InputInjector
{
public:
    explicit InputInjectorVt(std::shared_ptr<VtMonitors> monitors, QObject* parent = nullptr);
    ~InputInjectorVt() final = default;

    // InputInjector implementation.
    void setScreenInfo(const QSize& screen_size, const QPoint& offset) final;
    void setBlockInput(bool enable) final;
    void injectKeyEvent(const proto::input::KeyEvent& event) final;
    void injectTextEvent(const proto::input::TextEvent& event) final;
    void injectMouseEvent(const proto::input::MouseEvent& event) final;
    void injectTouchEvent(const proto::input::TouchEvent& event) final;

private:
    std::shared_ptr<VtMonitors> monitors_;
    bool shift_ = false;
    bool ctrl_ = false;
    bool alt_ = false;

    // Pixel size of the rendered terminal (from setScreenInfo) and the last button mask, used to map
    // pointer coordinates to cells and to detect button press/release transitions.
    QSize screen_size_;
    quint32 last_buttons_ = 0;

    Q_DISABLE_COPY_MOVE(InputInjectorVt)
};

#endif // HOST_INPUT_INJECTOR_VT_H
