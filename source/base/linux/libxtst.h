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

#ifndef BASE_LINUX_LIBXTST_H
#define BASE_LINUX_LIBXTST_H

#include <QtClassHelperMacros>

// From <X11/Xlib.h>; forward-declared so callers do not pull X11 headers (and their macros, which clash
// with Qt) through this one.
typedef struct _XDisplay Display;

// Thin wrapper over the subset of libXtst (the XTEST extension) used to synthesize input. The library is
// loaded dynamically on first use and the resolved symbols are cached, so the binary does not link against
// libXtst. Every call returns a zero result if the library or a symbol is unavailable.
class LibXtst
{
    Q_DISABLE_COPY_MOVE(LibXtst)

public:
    // Loads libXtst and resolves its symbols once. Returns false if it is not available.
    static bool ensureLoaded();

    static int queryExtension(Display* display, int* event_base, int* error_base, int* major, int* minor);
    static int grabControl(Display* display, int impervious);
    static int fakeKeyEvent(Display* display, unsigned int keycode, int is_press, unsigned long delay);
    static int fakeButtonEvent(Display* display, unsigned int button, int is_press, unsigned long delay);
    static int fakeMotionEvent(Display* display, int screen, int x, int y, unsigned long delay);
};

#endif // BASE_LINUX_LIBXTST_H
