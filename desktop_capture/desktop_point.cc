//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/desktop_point.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/desktop_point.h"

namespace aspia {

DesktopPoint::DesktopPoint(int x, int y) :
    x_(x),
    y_(y)
{
    // Nothing
}

DesktopPoint::DesktopPoint(const DesktopPoint& point) :
    x_(point.x_),
    y_(point.y_)
{
    // Nothing
}

int DesktopPoint::x() const
{
    return x_;
}

int DesktopPoint::y() const
{
    return y_;
}

void DesktopPoint::Set(int x, int y)
{
    x_ = x;
    y_ = y;
}

bool DesktopPoint::IsEqual(const DesktopPoint& other) const
{
    return (x_ == other.x_ && y_ == other.y_);
}

void DesktopPoint::Translate(int x_offset, int y_offset)
{
    x_ += x_offset;
    y_ += y_offset;
}

DesktopPoint& DesktopPoint::operator=(const DesktopPoint& other)
{
    Set(other.x_, other.y_);
    return *this;
}

} // namespace aspia
