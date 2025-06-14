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

// Don't include this file in any .h files because it pulls in some X headers.

#ifndef BASE_DESKTOP_X11_X_SERVER_PIXEL_BUFFER_H
#define BASE_DESKTOP_X11_X_SERVER_PIXEL_BUFFER_H

#include "base/macros_magic.h"
#include "base/desktop/geometry.h"

#include <memory>
#include <vector>

#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>

namespace base {

class Frame;
class XAtomCache;

// A class to allow the X server's pixel buffer to be accessed as efficiently as possible.
class XServerPixelBuffer
{
public:
    XServerPixelBuffer();
    ~XServerPixelBuffer();

    void release();

    // Allocate (or reallocate) the pixel buffer for |window|. Returns false in case of an error
    // (e.g. window doesn't exist).
    bool init(XAtomCache* cache, Window window);

    bool isInitialized() { return window_ != 0; }

    // Returns the size of the window the buffer was initialized for.
    Size windowSize() { return window_rect_.size(); }

    // Returns the rectangle of the window the buffer was initialized for.
    const Rect& windowRect() { return window_rect_; }

    // Returns true if the window can be found.
    bool isWindowValid() const;

    // If shared memory is being used without pixmaps, synchronize this pixel buffer with the root
    // window contents (otherwise, this is a no-op).
    // This is to avoid doing a full-screen capture for each individual rectangle in the capture
    // list, when it only needs to be done once at the beginning.
    void synchronize();

    // Capture the specified rectangle and stores it in the |frame|. In the case where the
    // full-screen data is captured by synchronize(), this simply returns the pointer without doing
    // any more work. The caller must ensure that |rect| is not larger than windowSize().
    bool captureRect(const Rect& rect, Frame* frame);

private:
    void releaseSharedMemorySegment();

    void initShm(const XWindowAttributes& attributes);
    bool initPixmaps(int depth);

    Display* display_ = nullptr;
    Window window_ = 0;
    Rect window_rect_;
    XImage* x_image_ = nullptr;
    XShmSegmentInfo* shm_segment_info_ = nullptr;
    XImage* x_shm_image_ = nullptr;
    Pixmap shm_pixmap_ = 0;
    GC shm_gc_ = nullptr;
    bool xshm_get_image_succeeded_ = false;

    Q_DISABLE_COPY(XServerPixelBuffer)
};

} // namespace base

#endif // BASE_DESKTOP_X11_X_SERVER_PIXEL_BUFFER_H
