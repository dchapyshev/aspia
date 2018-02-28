//
// PROJECT:         Aspia
// FILE:            desktop_capture/mouse_cursor.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/mouse_cursor.h"

namespace aspia {

// static
std::unique_ptr<MouseCursor>
MouseCursor::Create(std::unique_ptr<uint8_t[]> data,
                    const DesktopSize& size,
                    const DesktopPoint& hotspot)
{
    return std::unique_ptr<MouseCursor>(
        new MouseCursor(std::move(data), size, hotspot));
}

MouseCursor::MouseCursor(std::unique_ptr<uint8_t[]> data,
                         const DesktopSize& size,
                         const DesktopPoint& hotspot) :
    data_(std::move(data)),
    size_(size),
    hotspot_(hotspot)
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

int MouseCursor::Stride() const
{
    return size_.Width() * sizeof(uint32_t);
}

bool MouseCursor::IsEqual(const MouseCursor& other)
{
    if (size_.IsEqual(other.size_) &&
        hotspot_.IsEqual(other.hotspot_) &&
        memcmp(data_.get(), other.data_.get(), Stride() * size_.Height()) == 0)
    {
        return true;
    }

    return false;
}

} // namespace aspia
