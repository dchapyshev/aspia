//
// PROJECT:         Aspia Remote Desktop
// FILE:            gui/video_window.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_GUI__VIDEO_WINDOW_H
#define _ASPIA_GUI__VIDEO_WINDOW_H

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlcrack.h>
#include <atlmisc.h>
#include <functional>

#include "base/lock.h"
#include "desktop_capture/dib_buffer.h"
#include "client/client.h"
#include "gui/timer.h"

namespace aspia {

class VideoWindow : public CWindowImpl<VideoWindow, CWindow>
{
public:
    BEGIN_MSG_MAP(VideoWindow)
        MSG_WM_PAINT(OnPaint)
        MSG_WM_ERASEBKGND(OnEraseBackground)

        MESSAGE_RANGE_HANDLER(WM_MOUSEFIRST, WM_MOUSELAST, OnMouse)

        MSG_WM_SIZE(OnSize)
        MSG_WM_WINDOWPOSCHANGED(OnWindowPosChanged)
        MSG_WM_TIMER(OnTimer)
        MSG_WM_HSCROLL(OnHScroll)
        MSG_WM_VSCROLL(OnVScroll)
        MSG_WM_CREATE(OnCreate)
        MSG_WM_MOUSELEAVE(OnMouseLeave)
    END_MSG_MAP()

    VideoWindow(Client *client);
    ~VideoWindow();

    void OnVideoUpdate(const uint8_t *buffer, const DesktopRegion &region);
    void OnVideoResize(const DesktopSize &size, const PixelFormat &format);

    void HasFocus(bool has)
    {
        has_focus_ = has;
    }

    CSize GetSize()
    {
        return CSize(buffer_.Size().Width(), buffer_.Size().Height());
    }

    LRESULT OnMouse(UINT msg, WPARAM wParam, LPARAM lParam, BOOL &handled);

private:
    LRESULT OnCreate(LPCREATESTRUCT create_struct);
    void OnPaint(CDCHandle dc);
    BOOL OnEraseBackground(CDCHandle dc);
    void OnSize(UINT type, CSize size);
    void OnWindowPosChanged(LPWINDOWPOS pos);
    void OnMouseLeave();
    LRESULT OnTimer(UINT_PTR event_id);
    void OnHScroll(UINT code, UINT pos, CScrollBar scrollbar);
    void OnVScroll(UINT code, UINT pos, CScrollBar scrollbar);

    void DrawBackground(CPaintDC &target_dc);
    void UpdateScrollBars();
    bool Scroll(LONG x, LONG y);
    bool Scroll(const CPoint &delta);

private:
    Client *client_;

    CBrush background_brush_;

    CSize client_size_; // // Размер клиентской области окна.
    CPoint center_offset_;
    CPoint scroll_pos_;

    ScopedCreateDC screen_dc_;

    DibBuffer buffer_;
    Lock buffer_lock_;

    CPoint prev_pos_;
    uint8_t prev_mask_;

    bool has_mouse_; // Находится ли курсор над окном.
    bool has_focus_; // Находится ли окно в фокусе.

    Timer scroll_timer_;
    CPoint scroll_delta_;
};

} // namespace aspia

#endif // _ASPIA_GUI__VIDEO_WINDOW_H
