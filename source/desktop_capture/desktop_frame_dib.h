//
// PROJECT:         Aspia
// FILE:            desktop_capture/desktop_frame_dib.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__DESKTOP_FRAME_DIB_H
#define _ASPIA_DESKTOP_CAPTURE__DESKTOP_FRAME_DIB_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <memory>

#include "base/win/scoped_gdi_object.h"
#include "desktop_capture/desktop_frame.h"

namespace aspia {

class DesktopFrameDIB : public DesktopFrame
{
public:
    ~DesktopFrameDIB() = default;

    static std::unique_ptr<DesktopFrameDIB> create(const QSize& size,
                                                   const PixelFormat& format,
                                                   HDC hdc);

    HBITMAP bitmap() { return bitmap_; }

private:
    DesktopFrameDIB(const QSize& size,
                    const PixelFormat& format,
                    int stride,
                    quint8* data,
                    HBITMAP bitmap);

    ScopedHBITMAP bitmap_;

    Q_DISABLE_COPY(DesktopFrameDIB)
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__DESKTOP_FRAME_DIB_H
