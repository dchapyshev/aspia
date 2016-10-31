/*
 * PROJECT:         Aspia Remote Desktop
 * FILE:            desktop_capture/desktop_rect.h
 * LICENSE:         See top-level directory
 * PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
 */

#ifndef _ASPIA_DESKTOP_CAPTURE__DESKTOP_RECT_H
#define _ASPIA_DESKTOP_CAPTURE__DESKTOP_RECT_H

#include "aspia_config.h"

#include <stdint.h>
#include <algorithm>

#include "desktop_capture/desktop_size.h"

class DesktopRect
{
public:
    DesktopRect();
    DesktopRect(const DesktopRect &rect);
    ~DesktopRect();

    static DesktopRect MakeXYWH(int32_t x, int32_t y, int32_t width, int32_t height);
    static DesktopRect MakeWH(int32_t width, int32_t height);
    static DesktopRect MakeLTRB(int32_t l, int32_t t, int32_t r, int32_t b);
    static DesktopRect MakeSize(const DesktopSize &size);

    int32_t left() const;
    int32_t top() const;
    int32_t right() const;
    int32_t bottom() const;

    int32_t x() const;
    int32_t y() const;
    int32_t width() const;
    int32_t height() const;

    void SetWidth(int width);
    void SetHeight(int height);

    bool is_empty() const;
    bool IsValid() const;

    bool IsEqualTo(const DesktopRect &other) const;
    DesktopSize size() const;
    bool Contains(int32_t x, int32_t y) const;
    bool ContainsRect(const DesktopRect &rect) const;
    void Translate(int32_t dx, int32_t dy);
    void IntersectWith(const DesktopRect &rect);

    //
    // Enlarges current DesktopRect by subtracting |left_offset| and |top_offset|
    // from |left_| and |top_|, and adding |right_offset| and |bottom_offset| to
    // |right_| and |bottom_|. This function does not normalize the result, so
    // |left_| and |top_| may be less than zero or larger than |right_| and
    // |bottom_|.
    //
    void Extend(int32_t left_offset, int32_t top_offset, int32_t right_offset, int32_t bottom_offset);

    DesktopRect& operator=(const DesktopRect &rc);
    bool operator==(const DesktopRect &rc);
    bool operator!=(const DesktopRect &rc);

private:
    DesktopRect(int32_t l, int32_t t, int32_t r, int32_t b);

private:
    int32_t left_;
    int32_t top_;
    int32_t right_;
    int32_t bottom_;
};

#endif // _ASPIA_DESKTOP_CAPTURE__DESKTOP_RECT_H
