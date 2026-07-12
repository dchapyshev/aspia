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

#ifndef HOST_INPUT_INJECTOR_MAC_H
#define HOST_INPUT_INJECTOR_MAC_H

#include <QPoint>
#include <QSet>

#include "host/input_injector.h"

class InputInjectorMac final : public InputInjector
{
public:
    explicit InputInjectorMac(QObject* parent = nullptr);
    ~InputInjectorMac() final;

    static InputInjectorMac* create(QObject* parent = nullptr);

    // InputInjector implementation.
    void setScreenInfo(const QSize& screen_size, const QPoint& offset) final;
    void setBlockInput(bool enable) final;
    void injectKeyEvent(const proto::input::KeyEvent& event) final;
    void injectTextEvent(const proto::input::TextEvent& event) final;
    void injectMouseEvent(const proto::input::MouseEvent& event) final;
    void injectTouchEvent(const proto::input::TouchEvent& event) final;

private:
    // Global display coordinates (top-left of the main display is the origin) the capturer/client
    // coordinates map into.
    QPoint screen_offset_;

    // Last posted cursor position, used to decide between a move and a plain button event.
    QPoint last_mouse_pos_ { -1, -1 };

    // Previous button mask, used to detect press/release transitions.
    quint32 last_mouse_mask_ = 0;

    // USB HID codes of the keys currently held, so the destructor can release them.
    QSet<quint32> pressed_keys_;

    Q_DISABLE_COPY(InputInjectorMac)
};

#endif // HOST_INPUT_INJECTOR_MAC_H
