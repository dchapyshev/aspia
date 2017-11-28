//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/mouse_cursor.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__MOUSE_CURSOR_H
#define _ASPIA_DESKTOP_CAPTURE__MOUSE_CURSOR_H

#include <memory>

#include "desktop_capture/desktop_geometry.h"

namespace aspia {

class MouseCursor
{
public:
    ~MouseCursor() = default;

    static std::unique_ptr<MouseCursor> Create(std::unique_ptr<uint8_t[]> data,
                                               const DesktopSize& size,
                                               const DesktopPoint& hotspot);

    const DesktopSize& Size() const;
    const DesktopPoint& Hotspot() const;
    const uint8_t* Data() const;

    int Stride() const;

    bool IsEqual(const MouseCursor& other);

private:
    MouseCursor(std::unique_ptr<uint8_t[]> data,
                const DesktopSize& size,
                const DesktopPoint& hotspot);

    std::unique_ptr<uint8_t[]> const data_;
    const DesktopSize size_;
    const DesktopPoint hotspot_;
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__MOUSE_CURSOR_H
