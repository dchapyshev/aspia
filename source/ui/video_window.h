//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/video_window.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__VIDEO_WINDOW_H
#define _ASPIA_UI__VIDEO_WINDOW_H

#include "base/scoped_hdc.h"
#include "desktop_capture/desktop_frame_dib.h"
#include "desktop_capture/mouse_cursor.h"
#include "ui/base/timer.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlmisc.h>

namespace aspia {

class UiVideoWindow : public CWindowImpl<UiVideoWindow, CWindow>
{
public:
    class Delegate
    {
    public:
        virtual ~Delegate() = default;
        virtual void OnPointerEvent(const DesktopPoint& pos, uint32_t mask) = 0;
    };

    explicit UiVideoWindow(Delegate* delegate);

    DesktopFrame* Frame();
    void DrawFrame();
    void ResizeFrame(const DesktopSize& size, const PixelFormat& format);
    DesktopSize FrameSize();

    void HasFocus(bool has);

private:
    BEGIN_MSG_MAP(UiVideoWindow)
        MESSAGE_RANGE_HANDLER(WM_MOUSEFIRST, WM_MOUSELAST, OnMouse)
        MESSAGE_HANDLER(WM_ERASEBKGND, OnSkipMessage)
        MESSAGE_HANDLER(WM_PAINT, OnPaint)
        MESSAGE_HANDLER(WM_WINDOWPOSCHANGED, OnSize)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        MESSAGE_HANDLER(WM_MOUSELEAVE, OnMouseLeave)
        MESSAGE_HANDLER(WM_TIMER, OnTimer)
        MESSAGE_HANDLER(WM_HSCROLL, OnHScroll)
        MESSAGE_HANDLER(WM_VSCROLL, OnVScroll)
    END_MSG_MAP()

    LRESULT OnSkipMessage(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnMouse(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnPaint(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnSize(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnMouseLeave(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnTimer(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnHScroll(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnVScroll(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);

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

    CTimer scroll_timer_;
    CPoint scroll_delta_;

    DISALLOW_COPY_AND_ASSIGN(UiVideoWindow);
};

} // namespace aspia

#endif // _ASPIA_UI__VIDEO_WINDOW_H
