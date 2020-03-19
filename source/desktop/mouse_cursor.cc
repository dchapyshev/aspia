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

#include "desktop/mouse_cursor.h"

namespace desktop {

MouseCursor::MouseCursor(base::ByteArray&& image, const Size& size, const Point& hotspot)
    : image_(std::move(image)),
      size_(size),
      hotspot_(hotspot)
{
    // Nothing
}

int MouseCursor::stride() const
{
    return size_.width() * sizeof(uint32_t);
}

bool MouseCursor::isEqual(const MouseCursor& other)
{
    return (size_ == other.size_ && hotspot_ == other.hotspot_ &&
            base::compare(image_, other.image_) == 0);
}

} // namespace desktop
