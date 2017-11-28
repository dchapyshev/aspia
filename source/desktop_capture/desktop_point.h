//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/desktop_point.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__DESKTOP_POINT_H
#define _ASPIA_DESKTOP_CAPTURE__DESKTOP_POINT_H

#include <cstdint>

namespace aspia {

class DesktopPoint
{
public:
    DesktopPoint() = default;

    DesktopPoint(int32_t x, int32_t y)
        : x_(x),
          y_(y)
    {
        // Nothing
    }

    DesktopPoint(const DesktopPoint& point)
        : x_(point.x_),
          y_(point.y_)
    {
        // Nothing
    }

    ~DesktopPoint() = default;

    int32_t x() const { return x_; }
    int32_t y() const { return y_; }

    void Set(int32_t x, int32_t y)
    {
        x_ = x;
        y_ = y;
    }

    bool IsEqual(const DesktopPoint& other) const
    {
        return (x_ == other.x_ && y_ == other.y_);
    }

    void Translate(int32_t x_offset, int32_t y_offset)
    {
        x_ += x_offset;
        y_ += y_offset;
    }

    DesktopPoint& operator=(const DesktopPoint& other)
    {
        Set(other.x_, other.y_);
        return *this;
    }

private:
    int32_t x_ = 0;
    int32_t y_ = 0;
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__DESKTOP_POINT_H
