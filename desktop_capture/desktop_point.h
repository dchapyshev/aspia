//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/desktop_point.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__DESKTOP_POINT_H
#define _ASPIA_DESKTOP_CAPTURE__DESKTOP_POINT_H

#include "aspia_config.h"

#include <stdint.h>

namespace aspia {

class DesktopPoint
{
public:
    DesktopPoint();
    DesktopPoint(int32_t x, int32_t y);
    DesktopPoint(const DesktopPoint& point);
    ~DesktopPoint() = default;

    int32_t x() const;
    int32_t y() const;

    void Set(int32_t x, int32_t y);

    bool IsEqual(const DesktopPoint& other) const;
    void Translate(int32_t x_offset, int32_t y_offset);

    DesktopPoint& operator=(const DesktopPoint& other);

private:
    int32_t x_;
    int32_t y_;
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__DESKTOP_POINT_H
