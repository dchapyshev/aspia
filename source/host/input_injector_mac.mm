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

#include "host/input_injector_mac.h"

#include "base/logging.h"

namespace host {

//--------------------------------------------------------------------------------------------------
InputInjectorMac::InputInjectorMac() = default;

//--------------------------------------------------------------------------------------------------
InputInjectorMac::~InputInjectorMac() = default;

//--------------------------------------------------------------------------------------------------
void InputInjectorMac::setScreenOffset(const QPoint& /* offset */)
{
    NOTIMPLEMENTED();
}

//--------------------------------------------------------------------------------------------------
void InputInjectorMac::setBlockInput(bool /* enable */)
{
    NOTIMPLEMENTED();
}

//--------------------------------------------------------------------------------------------------
void InputInjectorMac::injectKeyEvent(const proto::input::KeyEvent& /* event */)
{
    NOTIMPLEMENTED();
}

//--------------------------------------------------------------------------------------------------
void InputInjectorMac::injectTextEvent(const proto::input::TextEvent& /* event */)
{
    NOTIMPLEMENTED();
}

//--------------------------------------------------------------------------------------------------
void InputInjectorMac::injectMouseEvent(const proto::input::MouseEvent& /* event */)
{
    NOTIMPLEMENTED();
}

//--------------------------------------------------------------------------------------------------
void InputInjectorMac::injectTouchEvent(const proto::input::TouchEvent& /* event */)
{
    NOTIMPLEMENTED();
}

} // namespace host
