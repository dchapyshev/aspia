//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/mouse_cursor.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/mouse_cursor.h"

namespace aspia {

MouseCursor::MouseCursor() :
    data_size_(0)
{
    // Nothing
}

MouseCursor::~MouseCursor()
{
    // Nothing
}

const DesktopSize& MouseCursor::Size() const
{
    return size_;
}

void MouseCursor::SetSize(int32_t width, int32_t height)
{
    size_.Set(width, height);
}

const DesktopPoint& MouseCursor::Hotspot() const
{
    return hotspot_;
}

void MouseCursor::SetHotspot(int32_t x, int32_t y)
{
    hotspot_.Set(x, y);
}

uint8_t* MouseCursor::Data() const
{
    return data_.get();
}

size_t MouseCursor::DataSize() const
{
    return data_size_;
}

void MouseCursor::SetData(std::unique_ptr<uint8_t[]> data, size_t data_size)
{
    data_ = std::move(data);
    data_size_ = data_size;
}

bool MouseCursor::IsEqual(const MouseCursor &other)
{
    if (size_ == other.size_ && hotspot_ == other.hotspot_ &&
        memcmp(data_.get(), other.data_.get(), size_.Width() * size_.Height() * sizeof(uint32_t)) == 0)
    {
        return true;
    }

    return false;
}

} // namespace aspia
