//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/desktop_point.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__DESKTOP_POINT_H
#define _ASPIA_DESKTOP_CAPTURE__DESKTOP_POINT_H

#include <stdint.h>

namespace aspia {

class DesktopPoint
{
public:
    DesktopPoint() = default;
    DesktopPoint(int x, int y);
    DesktopPoint(const DesktopPoint& point);
    ~DesktopPoint() = default;

    int x() const;
    int y() const;

    void Set(int x, int y);

    bool IsEqual(const DesktopPoint& other) const;
    void Translate(int x_offset, int y_offset);

    DesktopPoint& operator=(const DesktopPoint& other);

private:
    int32_t x_ = 0;
    int32_t y_ = 0;
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__DESKTOP_POINT_H
