//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/desktop_frame.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/desktop_frame.h"
#include "base/scoped_select_object.h"
#include "base/logging.h"

namespace aspia {

DesktopFrame::DesktopFrame(const DesktopSize& size,
                           const PixelFormat& format,
                           int stride,
                           uint8_t* data) :
    size_(size),
    format_(format),
    stride_(stride),
    data_(data)
{
    // Nothing
}

DesktopFrame::~DesktopFrame()
{
    // Nothing
}

const DesktopSize& DesktopFrame::Size() const
{
    return size_;
}

const PixelFormat& DesktopFrame::Format() const
{
    return format_;
}

int DesktopFrame::Stride() const
{
    return stride_;
}

bool DesktopFrame::Contains(int32_t x, int32_t y)
{
    return (x > 0 && x <= size_.Width() && y > 0 && y <= size_.Height());
}

uint8_t* DesktopFrame::GetFrameData() const
{
    return data_;
}

uint8_t* DesktopFrame::GetFrameDataAtPos(const DesktopPoint& pos) const
{
    return GetFrameData() + Stride() * pos.y() + format_.BytesPerPixel() * pos.x();
}

uint8_t* DesktopFrame::GetFrameDataAtPos(int32_t x, int32_t y) const
{
    return GetFrameDataAtPos(DesktopPoint(x, y));
}

void DesktopFrame::InvalidateFrame() const
{
    updated_region_.AddRect(DesktopRect::MakeSize(size_));
}

const DesktopRegion& DesktopFrame::UpdatedRegion() const
{
    return updated_region_;
}

DesktopRegion* DesktopFrame::MutableUpdatedRegion()
{
    return &updated_region_;
}

} // namespace aspia
