//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/desktop_rect.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/desktop_rect.h"

#include <algorithm>

namespace aspia {

DesktopRect::DesktopRect() :
    left_(0),
    top_(0),
    right_(0),
    bottom_(0)
{
    // Nothing
}

DesktopRect::DesktopRect(const DesktopRect& other) :
    left_(other.left_),
    top_(other.top_),
    right_(other.right_),
    bottom_(other.bottom_)
{
    // Nothing
}

DesktopRect::DesktopRect(int32_t l, int32_t t, int32_t r, int32_t b) :
    left_(l),
    top_(t),
    right_(r),
    bottom_(b)
{
    // Nothing
}

// static
DesktopRect DesktopRect::MakeXYWH(int32_t x, int32_t y, int32_t width, int32_t height)
{
    return DesktopRect(x, y, x + width, y + height);
}

// static
DesktopRect DesktopRect::MakeWH(int32_t width, int32_t height)
{
    return DesktopRect(0, 0, width, height);
}

// static
DesktopRect DesktopRect::MakeLTRB(int32_t l, int32_t t, int32_t r, int32_t b)
{
    return DesktopRect(l, t, r, b);
}

// static
DesktopRect DesktopRect::MakeSize(const DesktopSize& size)
{
    return DesktopRect(0, 0, size.Width(), size.Height());
}

int32_t DesktopRect::Left() const
{
    return left_;
}

int32_t DesktopRect::Top() const
{
    return top_;
}

int32_t DesktopRect::Right() const
{
    return right_;
}

int32_t DesktopRect::Bottom() const
{
    return bottom_;
}

int32_t DesktopRect::x() const
{
    return left_;
}

int32_t DesktopRect::y() const
{
    return top_;
}

int32_t DesktopRect::Width() const
{
    return right_ - left_;
}

int32_t DesktopRect::Height() const
{
    return bottom_ - top_;
}

DesktopPoint DesktopRect::LeftTop() const
{
    return DesktopPoint(Left(), Top());
}

bool DesktopRect::IsEmpty() const
{
    return (Width() <= 0 || Height() <= 0);
}

bool DesktopRect::IsValid() const
{
    return (right_ < left_ && bottom_ < top_);
}

bool DesktopRect::IsEqual(const DesktopRect& other) const
{
    return (left_   == other.left_  &&
            top_    == other.top_   &&
            right_  == other.right_ &&
            bottom_ == other.bottom_);
}

DesktopSize DesktopRect::Size() const
{
    return DesktopSize(Width(), Height());
}

bool DesktopRect::Contains(int32_t x, int32_t y) const
{
    return (x >= left_ && x < right_ && y >= top_ && y < bottom_);
}

bool DesktopRect::ContainsRect(const DesktopRect& other) const
{
    return (other.left_ >= left_ && other.right_  <= right_ &&
            other.top_  >= top_  && other.bottom_ <= bottom_);
}

void DesktopRect::Translate(int32_t dx, int32_t dy)
{
    left_   += dx;
    right_  += dx;
    top_    += dy;
    bottom_ += dy;
}

void DesktopRect::IntersectWith(const DesktopRect& other)
{
    left_   = std::max(Left(),   other.Left());
    top_    = std::max(Top(),    other.Top());
    right_  = std::min(Right(),  other.Right());
    bottom_ = std::min(Bottom(), other.Bottom());

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
