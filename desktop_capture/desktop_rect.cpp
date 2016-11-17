/*
* PROJECT:         Aspia Remote Desktop
* FILE:            desktop_capture/desktop_rect.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "desktop_capture/desktop_rect.h"

#include <algorithm>

DesktopRect::DesktopRect() :
    left_(0),
    top_(0),
    right_(0),
    bottom_(0)
{
}

DesktopRect::DesktopRect(const DesktopRect &rect) :
    left_(rect.left_),
    top_(rect.top_),
    right_(rect.right_),
    bottom_(rect.bottom_)
{
}

DesktopRect::DesktopRect(int32_t l, int32_t t, int32_t r, int32_t b) :
    left_(l),
    top_(t),
    right_(r),
    bottom_(b)
{
}

DesktopRect::~DesktopRect() {}

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
DesktopRect DesktopRect::MakeSize(const DesktopSize &size)
{
    return DesktopRect(0, 0, size.width(), size.height());
}

int32_t DesktopRect::left() const
{
    return left_;
}

int32_t DesktopRect::top() const
{
    return top_;
}

int32_t DesktopRect::right() const
{
    return right_;
}

int32_t DesktopRect::bottom() const
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

int32_t DesktopRect::width() const
{
    return right_ - left_;
}

int32_t DesktopRect::height() const
{
    return bottom_ - top_;
}

void DesktopRect::SetWidth(int32_t width)
{
    right_ = left_ + width;
}

void DesktopRect::SetHeight(int32_t height)
{
    bottom_ = top_ + height;
}

bool DesktopRect::is_empty() const
{
    return (width() <= 0 || height() <= 0);
}

bool DesktopRect::IsValid() const
{
    return (right_ < left_ && bottom_ < top_);
}

bool DesktopRect::IsEqualTo(const DesktopRect &other) const
{
    return (left_   == other.left_  &&
            top_    == other.top_   &&
            right_  == other.right_ &&
            bottom_ == other.bottom_);
}

DesktopSize DesktopRect::size() const
{
    return DesktopSize(width(), height());
}

bool DesktopRect::Contains(int32_t x, int32_t y) const
{
    return (x >= left_ && x < right_ && y >= top_ && y < bottom_);
}

bool DesktopRect::ContainsRect(const DesktopRect &rect) const
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
    left_   = std::max(left(),   rect.left());
    top_    = std::max(top(),    rect.top());
    right_  = std::min(right(),  rect.right());
    bottom_ = std::min(bottom(), rect.bottom());

    if (is_empty())
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

DesktopRect& DesktopRect::operator=(const DesktopRect &other)
{
    left_   = other.left_;
    top_    = other.top_;
    right_  = other.right_;
    bottom_ = other.bottom_;

    return *this;
}

bool DesktopRect::operator==(const DesktopRect &other)
{
    return IsEqualTo(other);
}

bool DesktopRect::operator!=(const DesktopRect &other)
{
    return !IsEqualTo(other);
}
