//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/desktop_rect.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__DESKTOP_RECT_H
#define _ASPIA_DESKTOP_CAPTURE__DESKTOP_RECT_H

#include "desktop_capture/desktop_size.h"
#include "desktop_capture/desktop_point.h"

namespace aspia {

class DesktopRect
{
public:
    DesktopRect() = default;
    DesktopRect(const DesktopRect& other);
    ~DesktopRect() = default;

    static DesktopRect MakeXYWH(int32_t x, int32_t y,
                                int32_t width, int32_t height);
    static DesktopRect MakeWH(int32_t width, int32_t height);
    static DesktopRect MakeLTRB(int32_t l, int32_t t, int32_t r, int32_t b);
    static DesktopRect MakeSize(const DesktopSize& size);

    int32_t Left() const;
    int32_t Top() const;
    int32_t Right() const;
    int32_t Bottom() const;

    int32_t x() const;
    int32_t y() const;
    int32_t Width() const;
    int32_t Height() const;

    DesktopPoint LeftTop() const;

    bool IsEmpty() const;
    bool IsValid() const;

    bool IsEqual(const DesktopRect& other) const;
    DesktopSize Size() const;
    bool Contains(int32_t x, int32_t y) const;
    bool ContainsRect(const DesktopRect& other) const;
    void Translate(int32_t dx, int32_t dy);
    void IntersectWith(const DesktopRect& other);

    //
    // Enlarges current DesktopRect by subtracting |left_offset| and |top_offset|
    // from |left_| and |top_|, and adding |right_offset| and |bottom_offset| to
    // |right_| and |bottom_|. This function does not normalize the result, so
    // |left_| and |top_| may be less than zero or larger than |right_| and
    // |bottom_|.
    //
    void Extend(int32_t left_offset, int32_t top_offset,
                int32_t right_offset, int32_t bottom_offset);

    DesktopRect& operator=(const DesktopRect& other);

private:
    DesktopRect(int32_t l, int32_t t, int32_t r, int32_t b);

    int32_t left_   = 0;
    int32_t top_    = 0;
    int32_t right_  = 0;
    int32_t bottom_ = 0;
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__DESKTOP_RECT_H
