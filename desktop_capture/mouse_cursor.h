//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/mouse_cursor.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__MOUSE_CURSOR_H
#define _ASPIA_DESKTOP_CAPTURE__MOUSE_CURSOR_H

#include "aspia_config.h"

#include <memory>

#include "desktop_capture/desktop_size.h"
#include "desktop_capture/desktop_point.h"

namespace aspia {

class MouseCursor
{
public:
    MouseCursor(std::unique_ptr<uint8_t[]>&& data,
                int width, int height,
                int hotspot_x, int hotspot_y);
    ~MouseCursor() = default;

    const DesktopSize& Size() const;
    const DesktopPoint& Hotspot() const;
    const uint8_t* Data() const;

    bool IsEqual(const MouseCursor& other);

private:
    std::unique_ptr<uint8_t[]> data_;
    DesktopSize size_;
    DesktopPoint hotspot_;
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__MOUSE_CURSOR_H
