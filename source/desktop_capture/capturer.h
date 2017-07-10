//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/capturer.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__CAPTURER_H
#define _ASPIA_DESKTOP_CAPTURE__CAPTURER_H

#include "desktop_capture/desktop_frame.h"
#include "desktop_capture/desktop_region.h"
#include "desktop_capture/mouse_cursor.h"

namespace aspia {

class Capturer
{
public:
    virtual ~Capturer() = default;

    virtual const DesktopFrame* CaptureImage() = 0;
    virtual std::unique_ptr<MouseCursor> CaptureCursor() = 0;
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__CAPTURER_H
