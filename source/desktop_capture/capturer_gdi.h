//
// PROJECT:         Aspia
// FILE:            desktop_capture/capturer_gdi.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__CAPTURER_GDI_H
#define _ASPIA_DESKTOP_CAPTURE__CAPTURER_GDI_H

#include "desktop_capture/capturer.h"

#include "base/win/scoped_hdc.h"
#include "desktop_capture/desktop_frame_dib.h"
#include "desktop_capture/differ.h"
#include "desktop_capture/win/scoped_thread_desktop.h"

namespace aspia {

class CapturerGDI : public Capturer
{
public:
    ~CapturerGDI() = default;

    static std::unique_ptr<CapturerGDI> create();

    const DesktopFrame* captureImage() override;
    std::unique_ptr<MouseCursor> captureCursor() override;

private:
    typedef HRESULT(WINAPI * DwmEnableCompositionFunc)(UINT);

    CapturerGDI();
    bool prepareCaptureResources();

    ScopedThreadDesktop desktop_;
    QRect desktop_dc_rect_;

    std::unique_ptr<Differ> differ_;
    std::unique_ptr<ScopedGetDC> desktop_dc_;
    ScopedCreateDC memory_dc_;

    static const int kNumFrames = 2;
    int curr_frame_id_ = 0;

    std::unique_ptr<DesktopFrameDIB> frame_[kNumFrames];

    CURSORINFO prev_cursor_info_;

    Q_DISABLE_COPY(CapturerGDI)
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__CAPTURER_GDI_H
