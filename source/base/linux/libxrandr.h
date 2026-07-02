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

#ifndef BASE_LINUX_LIBXRANDR_H
#define BASE_LINUX_LIBXRANDR_H

#include <QtClassHelperMacros>

#include "base/linux/x11_headers.h"

// Thin wrapper over the subset of libXrandr (the RANDR extension) used to track screen configuration
// changes. The library is loaded dynamically on first use and the resolved symbols are cached, so the
// binary does not link against libXrandr. Every call is a no-op if the library or a symbol is unavailable.
class LibXrandr
{
    Q_DISABLE_COPY_MOVE(LibXrandr)

public:
    // Loads libXrandr and resolves its symbols once. Returns false if it is not available.
    static bool ensureLoaded();

    static int queryExtension(Display* display, int* event_base, int* error_base);
    static int queryVersion(Display* display, int* major, int* minor);
    static void selectInput(Display* display, Window window, int mask);
    static int updateConfiguration(XEvent* event);

    // Monitor enumeration (XRandR 1.5+). getMonitors() returns nullptr if the extension is too old.
    static XRRMonitorInfo* getMonitors(Display* display, Window window, int get_active, int* nmonitors);
    static void freeMonitors(XRRMonitorInfo* monitors);
};

#endif // BASE_LINUX_LIBXRANDR_H
