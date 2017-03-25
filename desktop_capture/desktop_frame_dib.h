//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/desktop_frame_dib.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__DESKTOP_FRAME_DIB_H
#define _ASPIA_DESKTOP_CAPTURE__DESKTOP_FRAME_DIB_H

#include "aspia_config.h"

#include "desktop_capture/desktop_frame.h"
#include "base/scoped_gdi_object.h"

namespace aspia {

class DesktopFrameDib : public DesktopFrame
{
public:
    ~DesktopFrameDib();

    static DesktopFrameDib* Create(const DesktopSize& size, const PixelFormat& format, HDC hdc);

    HBITMAP Bitmap();

private:
    DesktopFrameDib(const DesktopSize& size,
                    const PixelFormat& format,
                    int stride,
                    uint8_t* data,
                    HBITMAP bitmap);

    ScopedBitmap bitmap_;

    DISALLOW_COPY_AND_ASSIGN(DesktopFrameDib);
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__DESKTOP_FRAME_DIB_H
