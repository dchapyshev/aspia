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

#ifndef HOST_INPUT_INJECTOR_MAC_H
#define HOST_INPUT_INJECTOR_MAC_H

#include "host/input_injector.h"

namespace host {

class InputInjectorMac final : public InputInjector
{
public:
    InputInjectorMac();
    ~InputInjectorMac();

    // InputInjector implementation.
    void setScreenOffset(const base::Point& offset) final;
    void setBlockInput(bool enable) final;
    void injectKeyEvent(const proto::KeyEvent& event) final;
    void injectMouseEvent(const proto::MouseEvent& event) final;
    void injectTouchEvent(const proto::TouchEvent& event) final;

private:
    Q_DISABLE_COPY(InputInjectorMac)
};

} // namespace host

#endif // HOST_INPUT_INJECTOR_MAC_H
