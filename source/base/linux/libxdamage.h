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

#ifndef BASE_LINUX_LIBXDAMAGE_H
#define BASE_LINUX_LIBXDAMAGE_H

#include <QtClassHelperMacros>

// From <X11/Xlib.h>; forward-declared so callers do not pull X11 headers (and their macros, which clash
// with Qt) through this one. Damage / Drawable / XserverRegion are all XID (unsigned long).
typedef struct _XDisplay Display;

// Thin wrapper over the subset of libXdamage (the DAMAGE extension) used to track changed screen regions.
// The library is loaded dynamically on first use and the resolved symbols are cached, so the binary does
// not link against libXdamage. Every call is a no-op if the library or a symbol is unavailable.
class LibXdamage
{
    Q_DISABLE_COPY_MOVE(LibXdamage)

public:
    // Loads libXdamage and resolves its symbols once. Returns false if it is not available.
    static bool ensureLoaded();

    static int queryExtension(Display* display, int* event_base, int* error_base);
    static unsigned long create(Display* display, unsigned long drawable, int level);
    static void destroy(Display* display, unsigned long damage);
    static void subtract(Display* display, unsigned long damage, unsigned long repair, unsigned long parts);
};

#endif // BASE_LINUX_LIBXDAMAGE_H
