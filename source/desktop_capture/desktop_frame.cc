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

uint8_t* DesktopFrame::GetFrameData() const
{
    return data_;
}

uint8_t* DesktopFrame::GetFrameDataAtPos(const QPoint& pos) const
{
    return GetFrameData() + stride() * pos.y() + format_.BytesPerPixel() * pos.x();
}

uint8_t* DesktopFrame::GetFrameDataAtPos(int32_t x, int32_t y) const
{
    return GetFrameDataAtPos(QPoint(x, y));
}

const QRegion& DesktopFrame::UpdatedRegion() const
{
    return updated_region_;
}

QRegion* DesktopFrame::MutableUpdatedRegion()
{
    return &updated_region_;
}

} // namespace aspia
