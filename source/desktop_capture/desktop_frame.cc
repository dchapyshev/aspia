//
// PROJECT:         Aspia
// FILE:            desktop_capture/desktop_frame.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/desktop_frame.h"
#include "base/logging.h"

namespace aspia {

DesktopFrame::DesktopFrame(const QSize& size,
                           const PixelFormat& format,
                           int stride,
                           uint8_t* data)
    : size_(size),
      format_(format),
      stride_(stride),
      data_(data)
{
    // Nothing
}

const QSize& DesktopFrame::size() const
{
    return size_;
}

const PixelFormat& DesktopFrame::format() const
{
    return format_;
}

int DesktopFrame::stride() const
{
    return stride_;
}

bool DesktopFrame::contains(int32_t x, int32_t y) const
{
    return (x > 0 && x <= size_.width() && y > 0 && y <= size_.height());
}

uint8_t* DesktopFrame::frameData() const
{
    return data_;
}

uint8_t* DesktopFrame::frameDataAtPos(const QPoint& pos) const
{
    return frameData() + stride() * pos.y() + format_.bytesPerPixel() * pos.x();
}

uint8_t* DesktopFrame::frameDataAtPos(int32_t x, int32_t y) const
{
    return frameDataAtPos(QPoint(x, y));
}

const QRegion& DesktopFrame::updatedRegion() const
{
    return updated_region_;
}

QRegion* DesktopFrame::mutableUpdatedRegion()
{
    return &updated_region_;
}

} // namespace aspia
