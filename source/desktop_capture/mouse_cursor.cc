//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "desktop_capture/mouse_cursor.h"

namespace desktop {

MouseCursor::MouseCursor(std::unique_ptr<uint8_t[]> data,
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
    return size_.width() * sizeof(uint32_t);
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

} // namespace desktop
