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

#ifndef BASE_DESKTOP_LINUX_WAYLAND_INPUT_TARGET_H
#define BASE_DESKTOP_LINUX_WAYLAND_INPUT_TARGET_H

#include <QtGlobal>

// Abstract input sink for Wayland sessions. Implemented by WaylandPortal (xdg-desktop-portal
// RemoteDesktop) and MutterScreenCast (org.gnome.Mutter.RemoteDesktop). Pointer coordinates are
// absolute within the captured stream; button and key codes are Linux evdev codes (X keycode minus 8
// for keys).
class WaylandInputTarget
{
public:
    virtual ~WaylandInputTarget() = default;

    virtual void notifyPointerMotionAbsolute(double x, double y) = 0;
    virtual void notifyPointerButton(qint32 button, bool pressed) = 0;
    virtual void notifyPointerAxis(double dx, double dy) = 0;
    virtual void notifyPointerAxisDiscrete(quint32 axis, qint32 steps) = 0;
    virtual void notifyKeyboardKeycode(qint32 keycode, bool pressed) = 0;
    virtual void notifyKeyboardKeysym(qint32 keysym, bool pressed) = 0;
};

#endif // BASE_DESKTOP_LINUX_WAYLAND_INPUT_TARGET_H
