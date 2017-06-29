//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/point.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__BASE__POINT_H
#define _ASPIA_UI__BASE__POINT_H

#include "desktop_capture/desktop_point.h"

namespace aspia {

class UiPoint
{
public:
    UiPoint()
    {
        memset(&pt_, 0, sizeof(pt_));
    }

    UiPoint(const UiPoint& other)
    {
        CopyFrom(other);
    }

    explicit UiPoint(const POINT& other)
    {
        CopyFrom(other);
    }

    UiPoint(int x, int y)
    {
        Set(x, y);
    }

    explicit UiPoint(LPARAM lparam)
    {
        Set(LOWORD(lparam), HIWORD(lparam));
    }

    POINT* Pointer()
    {
        return &pt_;
    }

    const POINT* ConstPointer() const
    {
        return &pt_;
    }

    void CopyFrom(const POINT& other)
    {
        pt_ = other;
    }

    void CopyFrom(const UiPoint& other)
    {
        CopyFrom(other.pt_);
    }

    void Set(int x, int y)
    {
        pt_.x = x;
        pt_.y = y;
    }

    int x() const
    {
        return pt_.x;
    }

    int y() const
    {
        return pt_.y;
    }

    bool IsEqual(const UiPoint& other)
    {
        return (x() == other.x() || y() == other.y());
    }

    void Translate(int x_offset, int y_offset)
    {
        pt_.x += x_offset;
        pt_.y += y_offset;
    }

    DesktopPoint desktop_point() const
    {
        return DesktopPoint(x(), y());
    }

private:
    POINT pt_;
};

} // namespace aspia

#endif // _ASPIA_UI__BASE__POINT_H
