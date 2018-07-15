//
// PROJECT:         Aspia
// FILE:            desktop_capture/screen_capturer_gdi.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef ASPIA_DESKTOP_CAPTURE__SCREEN_CAPTURER_GDI_H_
#define ASPIA_DESKTOP_CAPTURE__SCREEN_CAPTURER_GDI_H_

#include "desktop_capture/screen_capturer.h"

#include "base/win/scoped_hdc.h"
#include "desktop_capture/desktop_frame_dib.h"
#include "desktop_capture/differ.h"
#include "desktop_capture/win/scoped_thread_desktop.h"

namespace aspia {

class ScreenCapturerGDI : public ScreenCapturer
{
public:
    ~ScreenCapturerGDI() = default;

    static ScreenCapturerGDI* create();

    const DesktopFrame* captureImage() override;
    std::unique_ptr<MouseCursor> captureCursor() override;

private:
    typedef HRESULT(WINAPI * DwmEnableCompositionFunc)(UINT);

    ScreenCapturerGDI();
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

    Q_DISABLE_COPY(ScreenCapturerGDI)
};

} // namespace aspia

#endif // ASPIA_DESKTOP_CAPTURE__SCREEN_CAPTURER_GDI_H_
