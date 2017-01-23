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
    MouseCursor();
    ~MouseCursor();

    const DesktopSize& Size() const;
    void SetSize(int32_t width, int32_t height);

    const DesktopPoint& Hotspot() const;
    void SetHotspot(int32_t x, int32_t y);

    uint8_t* Data() const;
    size_t DataSize() const;
    void SetData(std::unique_ptr<uint8_t[]> data, size_t data_size);

    bool IsEqual(const MouseCursor &other);

private:
    std::unique_ptr<uint8_t[]> data_;
    size_t data_size_;

    DesktopSize size_;
    DesktopPoint hotspot_;
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__MOUSE_CURSOR_H
