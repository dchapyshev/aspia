//
// PROJECT:         Aspia
// FILE:            desktop_capture/desktop_frame.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/desktop_frame.h"

namespace aspia {

DesktopFrame::DesktopFrame(const QSize& size,
                           const PixelFormat& format,
                           int stride,
                           quint8* data)
    : size_(size),
      format_(format),
      stride_(stride),
      data_(data)
{
    // Nothing
}

bool DesktopFrame::contains(int x, int y) const
{
    return (x >= 0 && x <= size_.width() && y >= 0 && y <= size_.height());
}

quint8* DesktopFrame::frameDataAtPos(const QPoint& pos) const
{
    return frameData() + stride() * pos.y() + format_.bytesPerPixel() * pos.x();
}

quint8* DesktopFrame::frameDataAtPos(int x, int y) const
{
    return frameDataAtPos(QPoint(x, y));
}

} // namespace aspia
