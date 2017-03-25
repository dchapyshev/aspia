//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/capturer_gdi.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__CAPTURER_GDI_H
#define _ASPIA_DESKTOP_CAPTURE__CAPTURER_GDI_H

#include "aspia_config.h"

#include "desktop_capture/capturer.h"
#include "desktop_capture/desktop_frame_dib.h"
#include "desktop_capture/differ.h"
#include "base/macros.h"
#include "base/scoped_thread_desktop.h"
#include "base/scoped_gdi_object.h"
#include "base/scoped_hdc.h"
#include "base/desktop.h"

namespace aspia {

//
// Класс захвата избражения экрана
// TODO: Захват изображения курсора
//
class CapturerGDI : public Capturer
{
public:
    CapturerGDI();
    ~CapturerGDI() = default;

    //
    // Метод выполнения захвата экрана
    // Возвращает указатель на буфер, который содержит изображение экрана.
    //
    const DesktopFrame* CaptureImage(bool* desktop_change) override;

    MouseCursor* CaptureCursor() override;

private:
    bool PrepareInputDesktop();
    void PrepareCaptureResources();

private:
    ScopedThreadDesktop desktop_;
    DesktopRect screen_rect_;

    std::unique_ptr<Differ> differ_;
    std::unique_ptr<ScopedGetDC> desktop_dc_;
    ScopedCreateDC memory_dc_;

    static const int kNumFrames = 2;
    int curr_frame_id_;

    std::unique_ptr<DesktopFrameDib> frame_[kNumFrames];

    CURSORINFO prev_cursor_info_;

    DISALLOW_COPY_AND_ASSIGN(CapturerGDI);
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__CAPTURER_GDI_H
