//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__DESKTOP__MOUSE_CURSOR_H
#define BASE__DESKTOP__MOUSE_CURSOR_H

#include "base/desktop/geometry.h"
#include "base/memory/byte_array.h"

namespace base {

class MouseCursor
{
public:
    MouseCursor(ByteArray&& image, const Size& size, const Point& hotspot);

    MouseCursor(MouseCursor&& other) noexcept;
    MouseCursor& operator=(MouseCursor&& other) noexcept;

    MouseCursor(const MouseCursor& other) = default;
    MouseCursor& operator=(const MouseCursor& other) = default;

    ~MouseCursor() = default;

    const Size& size() const { return size_; }
    int width() const { return size_.width(); }
    int height() const { return size_.height(); }

    const Point& hotSpot() const { return hotspot_; }
    int hotSpotX() const { return hotspot_.x(); }
    int hotSpotY() const { return hotspot_.y(); }

    const ByteArray& constImage() const { return image_; }
    ByteArray& image() { return image_; }

    int stride() const;

    bool equals(const MouseCursor& other);

private:
    ByteArray image_;
    Size size_;
    Point hotspot_;
};

} // namespace base

#endif // BASE__DESKTOP__MOUSE_CURSOR_H
