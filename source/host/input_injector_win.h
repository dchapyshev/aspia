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

#ifndef HOST_INPUT_INJECTOR_WIN_H
#define HOST_INPUT_INJECTOR_WIN_H

#include <QSet>

#include "base/macros_magic.h"
#include "host/input_injector.h"
#include "host/win/touch_injector.h"

namespace host {

class InputInjectorWin final : public InputInjector
{
    Q_OBJECT

public:
    explicit InputInjectorWin(QObject* parent = nullptr);
    ~InputInjectorWin() final;

    // InputInjector implementation.
    void setScreenOffset(const base::Point& offset) final;
    void setBlockInput(bool enable) final;
    void injectKeyEvent(const proto::KeyEvent& event) final;
    void injectTextEvent(const proto::TextEvent& event) final;
    void injectMouseEvent(const proto::MouseEvent& event) final;
    void injectTouchEvent(const proto::TouchEvent& event) final;

private:
    void beforeInput();
    bool isCtrlAndAltPressed();
    void setBlockInputImpl(bool enable);

    bool block_input_ = false;
    QSet<quint32> pressed_keys_;

    base::Point screen_offset_;
    base::Point last_mouse_pos_;
    quint32 last_mouse_mask_ = 0;

    TouchInjector touch_injector_;

    DISALLOW_COPY_AND_ASSIGN(InputInjectorWin);
};

} // namespace host

#endif // HOST_INPUT_INJECTOR_WIN_H
