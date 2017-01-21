//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/cursor_capturer.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__CURSOR_CAPTURER_H
#define _ASPIA_DESKTOP_CAPTURE__CURSOR_CAPTURER_H

#include "aspia_config.h"

#include <memory>

#include "base/scoped_gdi_object.h"
#include "base/scoped_hdc.h"
#include "base/scoped_select_object.h"
#include "desktop_capture/desktop_size.h"
#include "desktop_capture/desktop_point.h"

namespace aspia {

class CursorCapturer
{
public:
    CursorCapturer();
    ~CursorCapturer();

    const uint8_t* Process();

private:
    HBITMAP GetCursorColorBits(uint8_t **bits, HCURSOR cursor, const DesktopSize &size);

    HBITMAP GetCursorBits(HCURSOR cursor,
                          uint8_t **MaskBits, size_t *MaskSize,
                          uint8_t **ColorBits, size_t *ColorSize,
                          DesktopPoint &hotspot,
                          DesktopSize &size);

private:
    std::unique_ptr<ScopedGetDC> desktop_dc_;
    ScopedCreateDC memory_dc_;
    ScopedHCURSOR prev_cursor_;
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__CURSOR_CAPTURER_H
