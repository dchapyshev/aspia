//
// PROJECT:         Aspia
// FILE:            desktop_capture/desktop_geometry.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
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

bool DesktopRect::Contains(int32_t x, int32_t y) const
{
    return (x >= left_ && x < right_ && y >= top_ && y < bottom_);
}

bool DesktopRect::ContainsRect(const DesktopRect& rect) const
{
    return (rect.left_ >= left_ && rect.right_  <= right_ &&
            rect.top_  >= top_  && rect.bottom_ <= bottom_);
}

void DesktopRect::Translate(int32_t dx, int32_t dy)
{
    left_   += dx;
    right_  += dx;
    top_    += dy;
    bottom_ += dy;
}

void DesktopRect::IntersectWith(const DesktopRect& rect)
{
    left_   = std::max(Left(),   rect.Left());
    top_    = std::max(Top(),    rect.Top());
    right_  = std::min(Right(),  rect.Right());
    bottom_ = std::min(Bottom(), rect.Bottom());

    if (IsEmpty())
    {
        left_   = 0;
        top_    = 0;
        right_  = 0;
        bottom_ = 0;
    }
}

void DesktopRect::Extend(int32_t left_offset,
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
