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

#ifndef BASE_X11_X11_HEADERS_H
#define BASE_X11_X11_HEADERS_H

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/XTest.h>

#if defined(CursorShape)
#undef CursorShape
#endif

#if defined(Expose)
#undef Expose
#endif

#if defined(FocusIn)
#undef FocusIn
#endif

#if defined(FocusOut)
#undef FocusOut
#endif

#if defined(FontChange)
#undef FontChange
#endif

#if defined(KeyPress)
#undef KeyPress
#endif

#if defined(KeyRelease)
#undef KeyRelease
#endif

#if defined(None)
#undef None
#endif

#if defined(Status)
#undef Status
#endif

#if defined(Bool)
#undef Bool
#endif

#if defined(True)
#undef True
#endif

#if defined(False)
#undef False
#endif

#define X11_Bool int
#define X11_Status int
#define X11_True 1
#define X11_False 0
#define X11_None 0
#define X11_CursorShape 0
#define X11_KeyPress 2
#define X11_KeyRelease 3
#define X11_FocusIn 9
#define X11_FocusOut 10
#define X11_FontChange 255
#define X11_Expose 12

#endif // BASE_X11_X11_HEADERS_H
