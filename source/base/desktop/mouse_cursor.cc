//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "base/desktop/mouse_cursor.h"

namespace base {

//--------------------------------------------------------------------------------------------------
MouseCursor::MouseCursor(QByteArray&& image, const Size& size, const Point& hotspot, const Point& dpi)
    : image_(std::move(image)),
      size_(size),
      hotspot_(hotspot),
      dpi_(dpi)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
MouseCursor::MouseCursor(MouseCursor&& other) noexcept
{
    *this = std::move(other);
}

//--------------------------------------------------------------------------------------------------
MouseCursor& MouseCursor::operator=(MouseCursor&& other) noexcept
{
    if (&other != this)
    {
        image_ = std::move(other.image_);
        size_ = other.size_;
        hotspot_ = other.hotspot_;
        dpi_ = other.dpi_;

        other.size_ = Size();
        other.hotspot_ = Point();
        other.dpi_ = Point();
    }

    return *this;
}

//--------------------------------------------------------------------------------------------------
int MouseCursor::stride() const
{
    return size_.width() * static_cast<int>(sizeof(quint32));
}

//--------------------------------------------------------------------------------------------------
bool MouseCursor::equals(const MouseCursor& other)
{
    return size_.equals(other.size_) &&
           hotspot_.equals(other.hotspot_) &&
           dpi_.equals(other.dpi_) &&
           image_ == other.image_;
}

} // namespace base
