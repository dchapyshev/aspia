//
// PROJECT:         Aspia
// FILE:            desktop_capture/mouse_cursor.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/mouse_cursor.h"

namespace aspia {

MouseCursor::MouseCursor(std::unique_ptr<quint8[]> data,
                         const QSize& size,
                         const QPoint& hotspot)
    : data_(std::move(data)),
      size_(size),
      hotspot_(hotspot)
{
    // Nothing
}

int MouseCursor::stride() const
{
    return size_.width() * sizeof(quint32);
}

bool MouseCursor::isEqual(const MouseCursor& other)
{
    if (size_ == other.size_ &&
        hotspot_ == other.hotspot_ &&
        memcmp(data_.get(), other.data_.get(), stride() * size_.height()) == 0)
    {
        return true;
    }

    return false;
}

} // namespace aspia
