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

#ifndef BASE_LINUX_LIBXEXT_H
#define BASE_LINUX_LIBXEXT_H

#include <QtClassHelperMacros>

#include "base/linux/x11_headers.h"

// Thin wrapper over the subset of libXext (the MIT-SHM extension) used to capture the screen through
// shared memory. The library is loaded dynamically on first use and the resolved symbols are cached, so
// the binary does not link against libXext. Every call is a no-op (returning a zero result) if the library
// or a symbol is unavailable.
class LibXext
{
    Q_DISABLE_COPY_MOVE(LibXext)

public:
    // Loads libXext and resolves its symbols once. Returns false if it is not available.
    static bool ensureLoaded();

    static int shmQueryVersion(Display* display, int* major, int* minor, int* shared_pixmaps);
    static XImage* shmCreateImage(
        Display* display, Visual* visual, unsigned int depth, int format, char* data,
        XShmSegmentInfo* shminfo, unsigned int width, unsigned int height);
    static int shmAttach(Display* display, XShmSegmentInfo* shminfo);
    static int shmPixmapFormat(Display* display);
    static Pixmap shmCreatePixmap(
        Display* display, Drawable drawable, char* data, XShmSegmentInfo* shminfo,
        unsigned int width, unsigned int height, unsigned int depth);
    static int shmGetImage(
        Display* display, Drawable drawable, XImage* image, int x, int y, unsigned long plane_mask);
};

#endif // BASE_LINUX_LIBXEXT_H
