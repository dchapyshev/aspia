//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/video_window.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/video_window.h"
#include "base/scoped_select_object.h"
#include "proto/desktop_session_message.pb.h"
#include "ui/base/module.h"

#include <algorithm>

namespace aspia {

static const UINT_PTR kScrollTimerId = 1;
static const int kScrollDelta = 25;
static const int kScrollTimerInterval = 20;
static const int kScrollBorder = 15;

static const uint8_t kWheelMask =
    proto::PointerEvent::WHEEL_DOWN | proto::PointerEvent::WHEEL_UP;

VideoWindow::VideoWindow(Delegate* delegate) :
    delegate_(delegate),
    background_brush_(CreateSolidBrush(RGB(25, 25, 25))),
    scroll_timer_(kScrollTimerId)
{
    // Nothing
}

void VideoWindow::FillBackgroundRect(HDC paint_dc, const DesktopRect& rect)
{
    if (!rect.IsEmpty())
    {
        RECT windows_rect = { rect.Left(), rect.Top(), rect.Right(), rect.Bottom() };

        FillRect(paint_dc, &windows_rect, background_brush_);
    }
}

void VideoWindow::DrawBackground(HDC paint_dc, const DesktopRect& paint_rect)
{
    DesktopRect left_rect =
        DesktopRect::MakeLTRB(paint_rect.Left(),
                              paint_rect.Top(),
                              center_offset_.x(),
                              paint_rect.Bottom());

    DesktopRect right_rect =
        DesktopRect::MakeLTRB(center_offset_.x() + frame_->Size().Width(),
                              paint_rect.Top(),
                              paint_rect.Right(),
                              paint_rect.Bottom());

    DesktopRect top_rect =
        DesktopRect::MakeLTRB(paint_rect.Left() + left_rect.Width(),
                              paint_rect.Top(),
                              paint_rect.Right() - right_rect.Width(),
                              center_offset_.y());

    DesktopRect bottom_rect =
        DesktopRect::MakeLTRB(paint_rect.Left() + left_rect.Width(),
                              center_offset_.y() + frame_->Size().Height(),
                              paint_rect.Right() - right_rect.Width(),
                              paint_rect.Bottom());

    FillBackgroundRect(paint_dc, left_rect);
    FillBackgroundRect(paint_dc, right_rect);
    FillBackgroundRect(paint_dc, top_rect);
    FillBackgroundRect(paint_dc, bottom_rect);
}

void VideoWindow::OnPaint()
{
    PAINTSTRUCT ps;
    HDC paint_dc = BeginPaint(hwnd(), &ps);

    DesktopRect paint_rect = DesktopRect::MakeLTRB(ps.rcPaint.left,
                                                   ps.rcPaint.top,
                                                   ps.rcPaint.right,
                                                   ps.rcPaint.bottom);

    if (frame_)
    {
        DesktopPoint scroll_pos(center_offset_.x() ? 0 : scroll_pos_.x(),
                                center_offset_.y() ? 0 : scroll_pos_.y());

        // When the window size is larger than the size of the remote screen, draw the background.
        DrawBackground(paint_dc, paint_rect);

        ScopedSelectObject select_object(memory_dc_, frame_->Bitmap());

        BitBlt(paint_dc,
               paint_rect.Left() + center_offset_.x(), paint_rect.Top() + center_offset_.y(),
               paint_rect.Width() - (center_offset_.x() * 2), paint_rect.Height() - (center_offset_.y() * 2),
               memory_dc_,
               paint_rect.Left() + scroll_pos.x(), paint_rect.Top() + scroll_pos.y(),
               SRCCOPY);
    }
    else
    {
        FillBackgroundRect(paint_dc, paint_rect);
    }

    EndPaint(hwnd(), &ps);
}

void VideoWindow::OnSize()
{
    if (!frame_)
        return;

    client_size_ = ClientSize();;

    int width = frame_->Size().Width();
    int height = frame_->Size().Height();

    center_offset_.Set((width < client_size_.Width()) ?
                           (client_size_.Width() / 2) - (width / 2) : 0,
                       (height < client_size_.Height()) ?
                           (client_size_.Height() / 2) - (height / 2) : 0);

    DesktopPoint scroll_pos(std::max(0, std::min(scroll_pos_.x(), width - client_size_.Width())),
                            std::max(0, std::min(scroll_pos_.y(), height - client_size_.Height())));

    ScrollWindowEx(hwnd(),
                   scroll_pos_.x() - scroll_pos.x(),
                   scroll_pos_.y() - scroll_pos.y(),
                   nullptr, nullptr, nullptr, nullptr,
                   SW_INVALIDATE);

    scroll_pos_ = scroll_pos;

    UpdateScrollBars(width, height);
}

void VideoWindow::OnMouse(UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (!frame_ || !has_focus_)
        return;

    if (!has_mouse_)
    {
        TRACKMOUSEEVENT tme;

        tme.cbSize = sizeof(TRACKMOUSEEVENT);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd();

        TrackMouseEvent(&tme);

        has_mouse_ = true;
    }

    POINT pos = { LOWORD(lParam), HIWORD(lParam) };

    DWORD flags = static_cast<DWORD>(wParam);

    uint8_t mask = ((flags & MK_LBUTTON) ? proto::PointerEvent::LEFT_BUTTON : 0) |
                   ((flags & MK_MBUTTON) ? proto::PointerEvent::MIDDLE_BUTTON : 0) |
                   ((flags & MK_RBUTTON) ? proto::PointerEvent::RIGHT_BUTTON : 0);

    WORD wheel_speed = 0;

    if (msg == WM_MOUSEWHEEL)
    {
        signed short speed = static_cast<signed short>(HIWORD(flags));

        if (speed < 0)
        {
            mask |= proto::PointerEvent::WHEEL_DOWN;
            wheel_speed = -speed / WHEEL_DELTA;
        }
        else
        {
            mask |= proto::PointerEvent::WHEEL_UP;
            wheel_speed = speed / WHEEL_DELTA;
        }

        if (!wheel_speed)
            wheel_speed = 1;

        if (!ScreenToClient(hwnd(), &pos))
            return;
    }
    else
    {
        int scroll_delta_x = 0;
        int scroll_delta_y = 0;

        if (client_size_.Width() < frame_->Size().Width())
        {
            if (pos.x > client_size_.Width() - kScrollBorder)
                scroll_delta_x = kScrollDelta;
            else if (pos.x < kScrollBorder)
                scroll_delta_x = -kScrollDelta;
        }

        if (client_size_.Height() < frame_->Size().Height())
        {
            if (pos.y > client_size_.Height() - kScrollBorder)
                scroll_delta_y = kScrollDelta;
            else if (pos.y < kScrollBorder)
                scroll_delta_y = -kScrollDelta;
        }

        scroll_delta_.Set(scroll_delta_x, scroll_delta_y);

        if (scroll_delta_x || scroll_delta_y)
            scroll_timer_.Start(hwnd(), kScrollTimerInterval);
        else
            scroll_timer_.Stop();
    }

    pos.x -= center_offset_.x() - std::abs(scroll_pos_.x());
    pos.y -= center_offset_.y() - std::abs(scroll_pos_.y());

    if (frame_->Contains(pos.x, pos.y))
    {
        DesktopPoint new_pos(pos.x, pos.y);

        if (!prev_pos_.IsEqual(new_pos) || mask != prev_mask_)
        {
            prev_pos_ = new_pos;
            prev_mask_ = mask & ~kWheelMask;

            if (mask & kWheelMask)
            {
                for (WORD i = 0; i < wheel_speed; ++i)
                {
                    delegate_->OnPointerEvent(new_pos, mask);
                    delegate_->OnPointerEvent(new_pos, mask & ~kWheelMask);
                }
            }
            else
            {
                delegate_->OnPointerEvent(new_pos, mask);
            }
        }
    }
}

void VideoWindow::OnMouseLeave()
{
    has_mouse_ = false;
}

LRESULT VideoWindow::OnTimer(UINT_PTR event_id)
{
    switch (event_id)
    {
        case kScrollTimerId:
        {
            if (!has_mouse_ || !Scroll(scroll_delta_.x(), scroll_delta_.y()))
            {
                scroll_timer_.Stop();
            }
        }
        break;
    }

    return 0;
}

void VideoWindow::OnHScroll(UINT code, UINT pos)
{
    switch (code)
    {
        case SB_LINEUP:
            Scroll(-2, 0);
            break;

        case SB_LINEDOWN:
            Scroll(2, 0);
            break;

        case SB_PAGEUP:
            Scroll(client_size_.Width() * -1 / 4, 0);
            break;

        case SB_PAGEDOWN:
            Scroll(client_size_.Width() * 1 / 4, 0);
            break;

        case SB_THUMBPOSITION:
        case SB_THUMBTRACK:
            Scroll(pos - scroll_pos_.x(), 0);
            break;
    }
}

void VideoWindow::OnVScroll(UINT code, UINT pos)
{
    switch (code)
    {
        case SB_LINEUP:
            Scroll(0, -2);
            break;

        case SB_LINEDOWN:
            Scroll(0, 2);
            break;

        case SB_PAGEUP:
            Scroll(0, client_size_.Height() * -1 / 4);
            break;

        case SB_PAGEDOWN:
            Scroll(0, client_size_.Height() * 1 / 4);
            break;

        case SB_THUMBPOSITION:
        case SB_THUMBTRACK:
            Scroll(0, pos - scroll_pos_.y());
            break;
    }
}

void VideoWindow::UpdateScrollBars(int width, int height)
{
    int scroolbar_width = 0;
    int scroolbar_height = 0;

    if (client_size_.Width() >= width)
        scroolbar_width = GetSystemMetrics(SM_CXHSCROLL);

    if (client_size_.Height() >= height)
        scroolbar_height = GetSystemMetrics(SM_CYVSCROLL);

    SCROLLINFO si = { 0 };
    si.cbSize = sizeof(si);
    si.fMask  = SIF_PAGE | SIF_POS | SIF_RANGE;

    si.nMax  = width;
    si.nPage = client_size_.Width() + scroolbar_width;
    si.nPos  = scroll_pos_.x();
    SetScrollInfo(hwnd(), SB_HORZ, &si, TRUE);

    si.nMax  = height;
    si.nPage = client_size_.Height() + scroolbar_height;
    si.nPos  = scroll_pos_.y();
    SetScrollInfo(hwnd(), SB_VERT, &si, TRUE);

    InvalidateRect(hwnd(), nullptr, FALSE);
}

bool VideoWindow::Scroll(int delta_x, int delta_y)
{
    if (!frame_)
        return false;

    int offset_x = 0;
    int offset_y = 0;

    int width = frame_->Size().Width();
    int height = frame_->Size().Height();

    if (delta_x)
    {
        offset_x = std::max(delta_x, -scroll_pos_.x());
        offset_x = std::min(offset_x, width - client_size_.Width() - scroll_pos_.x());
    }

    if (delta_y)
    {
        offset_y = std::max(delta_y, -scroll_pos_.y());
        offset_y = std::min(offset_y, height - client_size_.Height() - scroll_pos_.y());
    }

    if (offset_x || offset_y)
    {
        scroll_pos_.Translate(offset_x, offset_y);

        ScrollWindowEx(hwnd(),
                       -offset_x, -offset_y,
                       nullptr, nullptr, nullptr, nullptr,
                       SW_INVALIDATE);
        UpdateScrollBars(width, height);

        return true;
    }

    return false;
}

DesktopFrame* VideoWindow::Frame()
{
    return frame_.get();
}

void VideoWindow::DrawFrame()
{
    InvalidateRect(hwnd(), nullptr, FALSE);
}

DesktopSize VideoWindow::FrameSize()
{
    if (!frame_)
        return DesktopSize();

    return frame_->Size();
}

void VideoWindow::HasFocus(bool has)
{
    has_focus_ = has;
}

void VideoWindow::ResizeFrame(const DesktopSize& size, const PixelFormat& format)
{
    screen_dc_.Reset(CreateCompatibleDC(nullptr));
    memory_dc_.Reset(CreateCompatibleDC(screen_dc_));

    frame_ = DesktopFrameDIB::Create(size, format, memory_dc_);

    PostMessageW(hwnd(), WM_SIZE, 0, 0);
}

bool VideoWindow::OnMessage(UINT msg, WPARAM wparam, LPARAM lparam, LRESULT* result)
{
    switch (msg)
    {
        case WM_PAINT:
            OnPaint();
            break;

        case WM_ERASEBKGND:
            break;

        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
            OnMouse(msg, wparam, lparam);
            break;

        case WM_WINDOWPOSCHANGED:
        case WM_SIZE:
            OnSize();
            break;

        case WM_TIMER:
            OnTimer(static_cast<UINT_PTR>(wparam));
            break;

        case WM_HSCROLL:
            OnHScroll(LOWORD(wparam), HIWORD(wparam));
            break;

        case WM_VSCROLL:
            OnVScroll(LOWORD(wparam), HIWORD(wparam));
            break;

        case WM_MOUSELEAVE:
            OnMouseLeave();
            break;

        default:
            return false;
    }

    *result = 0;
    return true;
}

} // namespace aspia
