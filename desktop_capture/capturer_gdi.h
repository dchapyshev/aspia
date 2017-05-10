//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/capturer_gdi.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__CAPTURER_GDI_H
#define _ASPIA_DESKTOP_CAPTURE__CAPTURER_GDI_H

#include "desktop_capture/capturer.h"
#include "desktop_capture/desktop_frame_dib.h"
#include "desktop_capture/differ.h"
#include "base/scoped_thread_desktop.h"
#include "base/scoped_gdi_object.h"
#include "base/scoped_hdc.h"

namespace aspia {

class CapturerGDI : public Capturer
{
public:
    ~CapturerGDI() = default;

    static std::unique_ptr<CapturerGDI> Create();

    const DesktopFrame* CaptureImage() override;
    std::unique_ptr<MouseCursor> CaptureCursor() override;

private:
    CapturerGDI();
    bool PrepareCaptureResources();

    ScopedThreadDesktop desktop_;
    DesktopRect desktop_dc_rect_;

    std::unique_ptr<Differ> differ_;
    std::unique_ptr<ScopedGetDC> desktop_dc_;
    ScopedCreateDC memory_dc_;

    static const int kNumFrames = 2;
    int curr_frame_id_ = 0;

    std::unique_ptr<DesktopFrameDIB> frame_[kNumFrames];

    CURSORINFO prev_cursor_info_;

    DISALLOW_COPY_AND_ASSIGN(CapturerGDI);
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__CAPTURER_GDI_H
