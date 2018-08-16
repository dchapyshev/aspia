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

#include "desktop_capture/desktop_geometry.h"

#include <algorithm>

namespace aspia {

DesktopRect::DesktopRect(const DesktopRect& other)
    : left_(other.left_),
      top_(other.top_),
      right_(other.right_),
      bottom_(other.bottom_)
{
    // Nothing
}

bool DesktopRect::contains(int32_t x, int32_t y) const
{
    return (x >= left_ && x < right_ && y >= top_ && y < bottom_);
}

bool DesktopRect::containsRect(const DesktopRect& rect) const
{
    return (rect.left_ >= left_ && rect.right_  <= right_ &&
            rect.top_  >= top_  && rect.bottom_ <= bottom_);
}

void DesktopRect::translate(int32_t dx, int32_t dy)
{
    left_   += dx;
    right_  += dx;
    top_    += dy;
    bottom_ += dy;
}

void DesktopRect::intersectWith(const DesktopRect& rect)
{
    left_   = std::max(left(),   rect.left());
    top_    = std::max(top(),    rect.top());
    right_  = std::min(right(),  rect.right());
    bottom_ = std::min(bottom(), rect.bottom());

    if (isEmpty())
    {
        left_   = 0;
        top_    = 0;
        right_  = 0;
        bottom_ = 0;
    }
}

void DesktopRect::extend(int32_t left_offset,
                         int32_t top_offset,
                         int32_t right_offset,
                         int32_t bottom_offset)
{
    left_   -= left_offset;
    top_    -= top_offset;
    right_  += right_offset;
    bottom_ += bottom_offset;
}

DesktopRect& DesktopRect::operator=(const DesktopRect& other)
{
    left_   = other.left_;
    top_    = other.top_;
    right_  = other.right_;
    bottom_ = other.bottom_;

    return *this;
}

} // namespace aspia
