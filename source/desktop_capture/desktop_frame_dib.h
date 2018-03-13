//
// PROJECT:         Aspia
// FILE:            desktop_capture/desktop_frame_dib.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__DESKTOP_FRAME_DIB_H
#define _ASPIA_DESKTOP_CAPTURE__DESKTOP_FRAME_DIB_H

#include "desktop_capture/desktop_frame.h"
#include "base/scoped_gdi_object.h"

#include <memory>

namespace aspia {

class DesktopFrameDIB : public DesktopFrame
{
public:
    ~DesktopFrameDIB() = default;

    static std::unique_ptr<DesktopFrameDIB> Create(const QSize& size,
                                                   const PixelFormat& format,
                                                   HDC hdc);

    HBITMAP Bitmap();

private:
    DesktopFrameDIB(const QSize& size,
                    const PixelFormat& format,
                    int stride,
                    uint8_t* data,
                    HBITMAP bitmap);

    ScopedHBITMAP bitmap_;

    DISALLOW_COPY_AND_ASSIGN(DesktopFrameDIB);
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__DESKTOP_FRAME_DIB_H
