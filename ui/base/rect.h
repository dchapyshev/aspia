//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/rect.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__BASE__RECT_H
#define _ASPIA_UI__BASE__RECT_H

#include "desktop_capture/desktop_rect.h"
#include "ui/base/point.h"
#include "ui/base/size.h"

namespace aspia {

class UiRect
{
public:
    UiRect()
    {
        memset(&rect_, 0, sizeof(rect_));
    }

    explicit UiRect(const RECT& other)
    {
        CopyFrom(other);
    }

    UiRect(const UiRect& other)
    {
        CopyFrom(other);
    }

    static UiRect MakeXYWH(int x, int y, int width, int height)
    {
        return UiRect(x, y, x + width, y + height);
    }

    static UiRect MakeWH(int width, int height)
    {
        return UiRect(0, 0, width, height);
    }

    static UiRect MakeLTRB(int l, int t, int r, int b)
    {
        return UiRect(l, t, r, b);
    }

    RECT* Pointer()
    {
        return &rect_;
    }

    const RECT* ConstPointer() const
    {
        return &rect_;
    }

    void CopyFrom(const RECT& other)
    {
        rect_ = other;
    }

    void CopyFrom(const UiRect& other)
    {
        CopyFrom(other.rect_);
    }

    void CopyTo(RECT& other)
    {
        other = rect_;
    }

    int Left() const
    {
        return rect_.left;
    }

    int Top() const
    {
        return rect_.top;
    }

    int Right() const
    {
        return rect_.right;
    }

    int Bottom() const
    {
        return rect_.bottom;
    }

    int Width() const
    {
        return rect_.right - rect_.left;
    }

    int Height() const
    {
        return rect_.bottom - rect_.top;
    }

    bool IsEmpty() const
    {
        return (Width() <= 0 || Height() <= 0);
    }

    bool IsValid() const
    {
        return (Right() < Left() && Bottom() < Top());
    }

    bool IsEqual(const UiRect& other) const
    {
        return (Left()   == other.Left()  &&
                Top()    == other.Top()   &&
                Right()  == other.Right() &&
                Bottom() == other.Bottom());
    }

    bool Contains(int x, int y) const
    {
        return (x >= Left() && x < Right() && y >= Top() && y < Bottom());
    }

    bool Contains(const UiPoint& point) const
    {
        return Contains(point.x(), point.y());
    }

    bool ContainsRect(const UiRect& other) const
    {
        return (other.Left() >= Left() && other.Right() <= Right() &&
                other.Top() >= Top()  && other.Bottom() <= Bottom());
    }

    UiSize Size() const
    {
        return UiSize(Width(), Height());
    }

    UiPoint Pos() const
    {
        return UiPoint(Left(), Top());
    }

    DesktopRect desktop_rect() const
    {
        return DesktopRect::MakeLTRB(Left(), Top(), Right(), Bottom());
    }

private:
    UiRect(int left, int top, int right, int bottom)
    {
        rect_.left   = left;
        rect_.top    = top;
        rect_.right  = right;
        rect_.bottom = bottom;
    }

    RECT rect_;
};

} // namespace aspia

#endif // _ASPIA_UI__BASE__RECT_H
