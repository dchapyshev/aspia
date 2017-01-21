//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/dib_buffer.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__DIB_BUFFER_H
#define _ASPIA_DESKTOP_CAPTURE__DIB_BUFFER_H

#include "base/scoped_gdi_object.h"
#include "base/scoped_hdc.h"

#include "desktop_capture/desktop_size.h"
#include "desktop_capture/pixel_format.h"
#include "desktop_capture/desktop_rect.h"
#include "desktop_capture/desktop_point.h"
#include "desktop_capture/desktop_region.h"

namespace aspia {

class DibBuffer
{
public:
    DibBuffer();
    ~DibBuffer();

    void Resize(HDC hdc, const DesktopSize &size, const PixelFormat &format);

    const DesktopSize& Size() const;
    const PixelFormat& Format() const;

    void DrawTo(HDC target_dc, const DesktopPoint &dest, const DesktopPoint &src, const DesktopSize &size);
    void DrawFrom(HDC source_dc, const DesktopPoint &dest, const DesktopPoint &src, const DesktopSize &size);

    void CopyFrom(const uint8_t *buffer, const DesktopRect &rect);
    void CopyFrom(const uint8_t *buffer, const DesktopRegion &region);

    void CopyTo(uint8_t *buffer, const DesktopRect &rect);
    void CopyTo(uint8_t *buffer, const DesktopRegion &region);

    bool Contains(int32_t x, int32_t y);

private:
    void AllocateBuffer(int align = 32);
    void Copy(const uint8_t *src_buffer, uint8_t *dst_buffer, const DesktopRect &rect);

private:
    ScopedCreateDC memory_dc_;
    ScopedBitmap bitmap_;
    uint8_t *buffer_;

    DesktopSize size_;
    PixelFormat format_;

    int bytes_per_pixel_;
    int bytes_per_row_;
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__DIB_BUFFER_H
