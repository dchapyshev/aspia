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

#ifndef BASE_LINUX_LIBXFIXES_H
#define BASE_LINUX_LIBXFIXES_H

#include <QtClassHelperMacros>

#include "base/linux/x11_headers.h"

// Thin wrapper over the subset of libXfixes (the XFIXES extension) used for cursor shape capture, damage
// regions and selection-change notifications. The library is loaded dynamically on first use and the
// resolved symbols are cached, so the binary does not link against libXfixes. Every call is a no-op
// (returning a zero result) if the library or a symbol is unavailable.
class LibXfixes
{
    Q_DISABLE_COPY_MOVE(LibXfixes)

public:
    // Loads libXfixes and resolves its symbols once. Returns false if it is not available.
    static bool ensureLoaded();

    static int queryExtension(Display* display, int* event_base, int* error_base);
    static XFixesCursorImage* getCursorImage(Display* display);
    static void selectCursorInput(Display* display, Window window, unsigned long mask);
    static XserverRegion createRegion(Display* display, XRectangle* rectangles, int nrectangles);
    static void destroyRegion(Display* display, XserverRegion region);
    static XRectangle* fetchRegionAndBounds(
        Display* display, XserverRegion region, int* nrectangles, XRectangle* bounds);
    static void selectSelectionInput(
        Display* display, Window window, Atom selection, unsigned long mask);
};

#endif // BASE_LINUX_LIBXFIXES_H
