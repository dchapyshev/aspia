//
// SmartCafe Project
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

#ifndef HOST_WIN_TOUCH_INJECTOR_H
#define HOST_WIN_TOUCH_INJECTOR_H

#include <QMap>
#include <qt_windows.h>

#include "host/win/touch_injector_defines.h"
#include "proto/desktop.h"

namespace host {

class TouchInjector
{
public:
    TouchInjector();
    ~TouchInjector();

    bool isInitialized() const { return initialized_; }
    void injectTouchEvent(const proto::desktop::TouchEvent& event);

private:
    void addNewTouchPoints(const proto::desktop::TouchEvent& event);
    void moveTouchPoints(const proto::desktop::TouchEvent& event);
    void endTouchPoints(const proto::desktop::TouchEvent& event);
    void cancelTouchPoints(const proto::desktop::TouchEvent& event);

    HMODULE user32_library_ = nullptr;
    InitializeTouchInjectionFunction initialize_touch_injection_ = nullptr;
    InjectTouchInputFunction inject_touch_input_ = nullptr;

    bool initialized_ = false;

    // This is a naive implementation. Check if we can achieve
    // better performance by reducing the number of copies.
    // To reduce the number of copies, we can have a vector of
    // POINTER_TOUCH_INFO and a map from touch ID to index in the vector.
    // When removing points from the vector, just swap it with the last element
    // and resize the vector.
    // All the POINTER_TOUCH_INFOs are stored as "move" points.
    QMap<quint32, OWN_POINTER_TOUCH_INFO> touches_in_contact_;

    Q_DISABLE_COPY(TouchInjector)
};

} // namespace host

#endif // HOST_WIN_TOUCH_INJECTOR_H
