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
#include "desktop_capture/win/scoped_thread_desktop.h"
#include "desktop_capture/desktop_frame_dib.h"
#include "desktop_capture/differ.h"
#include "desktop_capture/screen_capture_frame_queue.h"

namespace aspia {

class ScreenCapturerGDI : public ScreenCapturer
{
public:
    ScreenCapturerGDI() = default;
    ~ScreenCapturerGDI() = default;

    bool screenList(ScreenList* screens) override;
    bool selectScreen(ScreenId screen_id) override;

    const DesktopFrame* captureFrame() override;

private:
    bool prepareCaptureResources();

    ScreenId current_screen_id_ = kFullDesktopScreenId;
    QString current_device_key_;

    ScopedThreadDesktop desktop_;
    QRect desktop_dc_rect_;

    std::unique_ptr<Differ> differ_;
    std::unique_ptr<ScopedGetDC> desktop_dc_;
    ScopedCreateDC memory_dc_;

    ScreenCaptureFrameQueue queue_;

    Q_DISABLE_COPY(ScreenCapturerGDI)
};

} // namespace aspia

#endif // ASPIA_DESKTOP_CAPTURE__SCREEN_CAPTURER_GDI_H_
