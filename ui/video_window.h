//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/video_window.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__VIDEO_WINDOW_H
#define _ASPIA_UI__VIDEO_WINDOW_H

#include "base/scoped_hdc.h"
#include "desktop_capture/desktop_frame_dib.h"
#include "desktop_capture/mouse_cursor.h"
#include "ui/base/child_window.h"
#include "ui/base/timer.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlmisc.h>

namespace aspia {

class UiVideoWindow : public UiChildWindow
{
public:
    class Delegate
    {
    public:
        virtual void OnPointerEvent(const DesktopPoint& pos, uint32_t mask) = 0;
    };

    explicit UiVideoWindow(Delegate* delegate);

    DesktopFrame* Frame();
    void DrawFrame();
    void ResizeFrame(const DesktopSize& size, const PixelFormat& format);
    DesktopSize FrameSize();

    void HasFocus(bool has);

    void OnMouse(UINT msg, WPARAM wparam, LPARAM lparam);

private:
    bool OnMessage(UINT msg, WPARAM wparam, LPARAM lparam, LRESULT* result) override;

    void OnPaint();
    void OnSize();
    void OnMouseLeave();
    LRESULT OnTimer(UINT_PTR event_id);
    void OnHScroll(UINT code, UINT pos);
    void OnVScroll(UINT code, UINT pos);

    void DrawBackground(HDC paint_dc, const CRect& paint_rect);
    void UpdateScrollBars(int width, int height);
    bool Scroll(LONG x, LONG y);

private:
    Delegate* delegate_;

    CBrush background_brush_;

    CSize client_size_; // The size of the client area of the window.
    CPoint center_offset_;
    CPoint scroll_pos_;

    ScopedCreateDC screen_dc_;
    ScopedCreateDC memory_dc_;

    std::unique_ptr<DesktopFrameDIB> frame_;

    CPoint prev_pos_;
    uint8_t prev_mask_ = 0;

    bool has_mouse_ = false; // Is the cursor over the window?
    bool has_focus_ = false; // Is the window in focus?

    UiTimer scroll_timer_;
    CPoint scroll_delta_;

    DISALLOW_COPY_AND_ASSIGN(UiVideoWindow);
};

} // namespace aspia

#endif // _ASPIA_UI__VIDEO_WINDOW_H
