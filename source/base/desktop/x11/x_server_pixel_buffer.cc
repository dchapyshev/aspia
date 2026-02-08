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

#include "base/desktop/x11/x_server_pixel_buffer.h"

#include "base/logging.h"
#include "base/desktop/frame.h"
#include "base/desktop/x11/x_atom_cache.h"
#include "base/desktop/x11/x_error_trap.h"

#include <stdint.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
bool getWindowRect(::Display* display, ::Window window, QRect* rect,
                   XWindowAttributes* attributes /* = nullptr */)
{
    XWindowAttributes localAttributes;
    int offsetX;
    int offsetY;

    if (attributes == nullptr)
        attributes = &localAttributes;

    {
        XErrorTrap errorTrap(display);
        if (!XGetWindowAttributes(display, window, attributes) ||
            errorTrap.lastErrorAndDisable() != 0)
        {
            return false;
        }
    }

    *rect = QRect(QPoint(attributes->x, attributes->y), QSize(attributes->width, attributes->height));

    {
        XErrorTrap errorTrap(display);
        ::Window child;
        if (!XTranslateCoordinates(display, window, attributes->root, -rect->left(),
                                   -rect->top(), &offsetX, &offsetY, &child) ||
            errorTrap.lastErrorAndDisable() != 0)
        {
            return false;
        }
    }
    rect->translate(offsetX, offsetY);
    return true;
}

//--------------------------------------------------------------------------------------------------
// Returns the number of bits |mask| has to be shifted left so its last
// (most-significant) bit set becomes the most-significant bit of the word.
// When |mask| is 0 the function returns 31.
quint32 maskToShift(quint32 mask)
{
    int shift = 0;

    if ((mask & 0xffff0000u) == 0)
    {
        mask <<= 16;
        shift += 16;
    }

    if ((mask & 0xff000000u) == 0)
    {
        mask <<= 8;
        shift += 8;
    }

    if ((mask & 0xf0000000u) == 0)
    {
        mask <<= 4;
        shift += 4;
    }

    if ((mask & 0xc0000000u) == 0)
    {
        mask <<= 2;
        shift += 2;
    }

    if ((mask & 0x80000000u) == 0)
        shift += 1;

    return shift;
}

//--------------------------------------------------------------------------------------------------
// Returns true if |image| is in RGB format.
bool isXImageRGBFormat(XImage* image)
{
  return image->bits_per_pixel == 32 && image->red_mask == 0xff0000 &&
         image->green_mask == 0xff00 && image->blue_mask == 0xff;
}

//--------------------------------------------------------------------------------------------------
// We expose two forms of blitting to handle variations in the pixel format.
// In FastBlit(), the operation is effectively a memcpy.
void fastBlit(XImage* x_image, uint8_t* src_pos, const QRect& rect, Frame* frame)
{
    DCHECK_LE(frame->topLeft().x(), rect.left());
    DCHECK_LE(frame->topLeft().y(), rect.top());

    int src_stride = x_image->bytes_per_line;
    int dst_x = rect.left() - frame->topLeft().x();
    int dst_y = rect.top() - frame->topLeft().y();

    quint8* dst_pos = frame->frameData() + frame->stride() * dst_y;
    dst_pos += dst_x * frame->format().bytesPerPixel();

    int height = rect.height();
    int row_bytes = rect.width() * frame->format().bytesPerPixel();

    for (int y = 0; y < height; ++y)
    {
        memcpy(dst_pos, src_pos, row_bytes);
        src_pos += src_stride;
        dst_pos += frame->stride();
    }
}

//--------------------------------------------------------------------------------------------------
void slowBlit(XImage* x_image, uint8_t* src_pos, const QRect& rect, Frame* frame)
{
    DCHECK_LE(frame->topLeft().x(), rect.left());
    DCHECK_LE(frame->topLeft().y(), rect.top());

    int src_stride = x_image->bytes_per_line;
    int dst_x = rect.left() - frame->topLeft().x();
    int dst_y = rect.top() - frame->topLeft().y();
    int width = rect.width(), height = rect.height();

    uint32_t red_mask = x_image->red_mask;
    uint32_t green_mask = x_image->red_mask;
    uint32_t blue_mask = x_image->blue_mask;

    uint32_t red_shift = maskToShift(red_mask);
    uint32_t green_shift = maskToShift(green_mask);
    uint32_t blue_shift = maskToShift(blue_mask);

    int bits_per_pixel = x_image->bits_per_pixel;

    uint8_t* dst_pos = frame->frameData() + frame->stride() * dst_y;
    dst_pos += dst_x * frame->format().bytesPerPixel();

    // TODO(hclam): Optimize, perhaps using MMX code or by converting to YUV directly.
    // TODO(sergeyu): This code doesn't handle XImage byte order properly and won't work with 24bpp
    // images. Fix it.
    for (int y = 0; y < height; y++)
    {
        uint32_t* dst_pos_32 = reinterpret_cast<uint32_t*>(dst_pos);
        uint32_t* src_pos_32 = reinterpret_cast<uint32_t*>(src_pos);
        uint16_t* src_pos_16 = reinterpret_cast<uint16_t*>(src_pos);

        for (int x = 0; x < width; x++)
        {
            // Dereference through an appropriately-aligned pointer.
            uint32_t pixel;

            if (bits_per_pixel == 32)
                pixel = src_pos_32[x];
            else if (bits_per_pixel == 16)
                pixel = src_pos_16[x];
            else
                pixel = src_pos[x];

            uint32_t r = (pixel & red_mask) << red_shift;
            uint32_t g = (pixel & green_mask) << green_shift;
            uint32_t b = (pixel & blue_mask) << blue_shift;

            // Write as 32-bit RGB.
            dst_pos_32[x] = ((r >> 8) & 0xff0000) | ((g >> 16) & 0xff00) | ((b >> 24) & 0xff);
        }

        dst_pos += frame->stride();
        src_pos += src_stride;
    }
}

}  // namespace

//--------------------------------------------------------------------------------------------------
XServerPixelBuffer::XServerPixelBuffer() = default;

//--------------------------------------------------------------------------------------------------
XServerPixelBuffer::~XServerPixelBuffer()
{
    release();
}

//--------------------------------------------------------------------------------------------------
void XServerPixelBuffer::release()
{
    if (x_image_)
    {
        XDestroyImage(x_image_);
        x_image_ = nullptr;
    }

    if (x_shm_image_)
    {
        XDestroyImage(x_shm_image_);
        x_shm_image_ = nullptr;
    }

    if (shm_pixmap_)
    {
        XFreePixmap(display_, shm_pixmap_);
        shm_pixmap_ = 0;
    }

    if (shm_gc_)
    {
        XFreeGC(display_, shm_gc_);
        shm_gc_ = nullptr;
    }

    releaseSharedMemorySegment();
    window_ = 0;
}

//--------------------------------------------------------------------------------------------------
void XServerPixelBuffer::releaseSharedMemorySegment()
{
    if (!shm_segment_info_)
        return;

    if (shm_segment_info_->shmaddr != nullptr)
        shmdt(shm_segment_info_->shmaddr);

    if (shm_segment_info_->shmid != -1)
        shmctl(shm_segment_info_->shmid, IPC_RMID, 0);

    delete shm_segment_info_;
    shm_segment_info_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
bool XServerPixelBuffer::init(XAtomCache* cache, Window window)
{
    release();
    display_ = cache->display();

    XWindowAttributes attributes;
    if (!getWindowRect(display_, window, &window_rect_, &attributes))
    {
        LOG(ERROR) << "getWindowRect failed";
        return false;
    }

    window_ = window;
    initShm(attributes);

    return true;
}

//--------------------------------------------------------------------------------------------------
void XServerPixelBuffer::initShm(const XWindowAttributes& attributes)
{
    Visual* default_visual = attributes.visual;
    int default_depth = attributes.depth;

    int major, minor;
    X11_Bool have_pixmaps;

    if (!XShmQueryVersion(display_, &major, &minor, &have_pixmaps))
    {
        LOG(INFO) << "Shared memory not supported. captureRect will use the XImage API instead";
        return;
    }

    bool using_shm = false;
    shm_segment_info_ = new XShmSegmentInfo;
    shm_segment_info_->shmid = -1;
    shm_segment_info_->shmaddr = nullptr;
    shm_segment_info_->readOnly = X11_False;

    x_shm_image_ = XShmCreateImage(display_, default_visual, default_depth,
                                   ZPixmap, 0, shm_segment_info_,
                                   window_rect_.width(), window_rect_.height());
    if (x_shm_image_)
    {
        shm_segment_info_->shmid =
            shmget(IPC_PRIVATE, x_shm_image_->bytes_per_line * x_shm_image_->height, IPC_CREAT | 0600);
        if (shm_segment_info_->shmid != -1)
        {
            void* shmat_result = shmat(shm_segment_info_->shmid, 0, 0);
            if (shmat_result != reinterpret_cast<void*>(-1))
            {
                shm_segment_info_->shmaddr = reinterpret_cast<char*>(shmat_result);
                x_shm_image_->data = shm_segment_info_->shmaddr;

                XErrorTrap error_trap(display_);
                using_shm = XShmAttach(display_, shm_segment_info_);
                XSync(display_, X11_False);

                if (error_trap.lastErrorAndDisable() != 0)
                    using_shm = false;

                if (using_shm)
                {
                    LOG(INFO) << "Using X shared memory segment " << shm_segment_info_->shmid;
                }
            }
        }
        else
        {
            LOG(ERROR) << "Failed to get shared memory segment. Performance may be degraded";
        }
    }

    if (!using_shm)
    {
        LOG(ERROR) << "Not using shared memory. Performance may be degraded.";
        releaseSharedMemorySegment();
        return;
    }

    if (have_pixmaps)
        have_pixmaps = initPixmaps(default_depth);

    shmctl(shm_segment_info_->shmid, IPC_RMID, 0);
    shm_segment_info_->shmid = -1;

    LOG(INFO) << "Using X shared memory extension v" << major << "." << minor
              << "with" << (have_pixmaps ? "" : "out") << "pixmaps.";
}

//--------------------------------------------------------------------------------------------------
bool XServerPixelBuffer::initPixmaps(int depth)
{
    int format = XShmPixmapFormat(display_);
    if (format != ZPixmap)
    {
        LOG(ERROR) << "Unsupported format:" << format;
        return false;
    }

    {
        XErrorTrap error_trap(display_);
        shm_pixmap_ = XShmCreatePixmap(display_, window_, shm_segment_info_->shmaddr,
                                       shm_segment_info_, window_rect_.width(), window_rect_.height(),
                                       depth);
        XSync(display_, X11_False);
        if (error_trap.lastErrorAndDisable() != 0)
        {
            LOG(ERROR) << "XShmCreatePixmap failed";

            // |shm_pixmap_| is not not valid because the request was not processed by the X Server,
            // so zero it.
            shm_pixmap_ = 0;
            return false;
        }
    }

    {
        XErrorTrap error_trap(display_);
        XGCValues shm_gc_values;

        shm_gc_values.subwindow_mode = IncludeInferiors;
        shm_gc_values.graphics_exposures = X11_False;

        shm_gc_ = XCreateGC(display_, window_, GCSubwindowMode | GCGraphicsExposures, &shm_gc_values);
        XSync(display_, X11_False);

        if (error_trap.lastErrorAndDisable() != 0)
        {
            LOG(ERROR) << "XCreateGC failed";

            XFreePixmap(display_, shm_pixmap_);
            shm_pixmap_ = 0;
            shm_gc_ = 0;  // See shm_pixmap_ comment above.
            return false;
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool XServerPixelBuffer::isWindowValid() const
{
    XWindowAttributes attributes;
    {
        XErrorTrap error_trap(display_);
        if (!XGetWindowAttributes(display_, window_, &attributes) ||
            error_trap.lastErrorAndDisable() != 0)
        {
            return false;
        }
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
void XServerPixelBuffer::synchronize()
{
    if (shm_segment_info_ && !shm_pixmap_)
    {
        // XShmGetImage can fail if the display is being reconfigured.
        XErrorTrap error_trap(display_);
        // XShmGetImage fails if the window is partially out of screen.
        xshm_get_image_succeeded_ =
            XShmGetImage(display_, window_, x_shm_image_, 0, 0, AllPlanes);
    }
}

//--------------------------------------------------------------------------------------------------
bool XServerPixelBuffer::captureRect(const QRect& rect, Frame* frame)
{
    DCHECK_LE(rect.right(), window_rect_.width());
    DCHECK_LE(rect.bottom(), window_rect_.height());

    XImage* image;
    quint8* data;

    if (shm_segment_info_ && (shm_pixmap_ || xshm_get_image_succeeded_))
    {
        if (shm_pixmap_)
        {
            XCopyArea(display_, window_, shm_pixmap_, shm_gc_, rect.left(), rect.top(),
                      rect.width(), rect.height(), rect.left(), rect.top());
            XSync(display_, X11_False);
        }

        image = x_shm_image_;
        data = reinterpret_cast<uint8_t*>(image->data) + rect.top() * image->bytes_per_line +
            rect.left() * image->bits_per_pixel / 8;

    }
    else
    {
        if (x_image_)
        XDestroyImage(x_image_);

        x_image_ = XGetImage(display_, window_, rect.left(), rect.top(), rect.width(), rect.height(),
                             AllPlanes, ZPixmap);
        if (!x_image_)
            return false;

        image = x_image_;
        data = reinterpret_cast<uint8_t*>(image->data);
    }

    if (isXImageRGBFormat(image))
    {
        fastBlit(image, data, rect, frame);
    }
    else
    {
        slowBlit(image, data, rect, frame);
    }

    return true;
}

} // namespace base
