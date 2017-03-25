//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/mouse_cursor.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/mouse_cursor.h"

namespace aspia {

MouseCursor::MouseCursor(std::unique_ptr<uint8_t[]>&& data,
                         int width, int height,
                         int hotspot_x, int hotspot_y) :
    data_(std::move(data)),
    size_(width, height),
    hotspot_(hotspot_x, hotspot_y)
{
    // Nothing
}

const DesktopSize& MouseCursor::Size() const
{
    return size_;
}

const DesktopPoint& MouseCursor::Hotspot() const
{
    return hotspot_;
}

const uint8_t* MouseCursor::Data() const
{
    return data_.get();
}

bool MouseCursor::IsEqual(const MouseCursor& other)
{
    if (size_.IsEqual(other.size_) &&
        hotspot_.IsEqual(other.hotspot_) &&
        memcmp(data_.get(), other.data_.get(), size_.Width() * size_.Height() * sizeof(uint32_t)) == 0)
    {
        return true;
    }

    return false;
}

} // namespace aspia
