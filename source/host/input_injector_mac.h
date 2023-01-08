//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

class InputInjectorMac : public InputInjector
{
public:
    InputInjectorMac();
    ~InputInjectorMac();

    // InputInjector implementation.
    void setScreenOffset(const base::Point& offset) override;
    void setBlockInput(bool enable) override;
    void injectKeyEvent(const proto::KeyEvent& event) override;
    void injectMouseEvent(const proto::MouseEvent& event) override;

private:
    DISALLOW_COPY_AND_ASSIGN(InputInjectorMac);
};

} // namespace host

#endif // HOST_INPUT_INJECTOR_MAC_H
