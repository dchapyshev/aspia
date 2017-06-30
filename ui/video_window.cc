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
#include <atlgdi.h>

namespace aspia {

static const UINT_PTR kScrollTimerId = 1;
static const int kScrollDelta = 25;
static const int kScrollTimerInterval = 20;
static const int kScrollBorder = 15;

static const uint8_t kWheelMask =
    proto::PointerEvent::WHEEL_DOWN | proto::PointerEvent::WHEEL_UP;

UiVideoWindow::UiVideoWindow(Delegate* delegate) :
    delegate_(delegate),
    background_brush_(CreateSolidBrush(RGB(25, 25, 25))),
    scroll_timer_(kScrollTimerId)
{
    // Nothing
}

void UiVideoWindow::DrawBackground(HDC paint_dc, const CRect& paint_rect)
{
    CRect left_rect(paint_rect.left,
                    paint_rect.top,
                    center_offset_.x,
                    paint_rect.bottom);

    CRect right_rect(center_offset_.x + frame_->Size().Width(),
                     paint_rect.top,
                     paint_rect.right,
                     paint_rect.bottom);

    CRect top_rect(paint_rect.left + left_rect.Width(),
                   paint_rect.top,
                   paint_rect.right - right_rect.Width(),
                   center_offset_.y);

    CRect bottom_rect(paint_rect.left + left_rect.Width(),
                      center_offset_.y + frame_->Size().Height(),
                      paint_rect.right - right_rect.Width(),
                      paint_rect.bottom);

    if (!left_rect.IsRectEmpty())
        FillRect(paint_dc, left_rect, background_brush_);

    if (!right_rect.IsRectEmpty())
        FillRect(paint_dc, right_rect, background_brush_);

    if (!top_rect.IsRectEmpty())
        FillRect(paint_dc, top_rect, background_brush_);

    if (!bottom_rect.IsRectEmpty())
        FillRect(paint_dc, bottom_rect, background_brush_);
}

void UiVideoWindow::OnPaint()
{
    CPaintDC paint(*this);

    if (frame_)
    {
        CPoint scroll_pos(center_offset_.x ? 0 : scroll_pos_.x,
                          center_offset_.y ? 0 : scroll_pos_.y);

        CRect paint_rect(paint.m_ps.rcPaint);

        // When the window size is larger than the size of the remote screen, draw the background.
        DrawBackground(paint.m_hDC, paint.m_ps.rcPaint);

        ScopedSelectObject select_object(memory_dc_, frame_->Bitmap());

        paint.BitBlt(paint_rect.left + center_offset_.x, paint_rect.top + center_offset_.y,
                     paint_rect.Width() - (center_offset_.x * 2), paint_rect.Height() - (center_offset_.y * 2),
                     memory_dc_,
                     paint_rect.left + scroll_pos.x,
                     paint_rect.top + scroll_pos.y,
                     SRCCOPY);
    }
    else
    {
        paint.FillRect(&paint.m_ps.rcPaint, background_brush_);
    }
}

void UiVideoWindow::OnSize()
{
    if (!frame_)
        return;

    CRect client_rect;
    GetClientRect(hwnd(), client_rect);

    client_size_ = client_rect.Size();

    int width = frame_->Size().Width();
    int height = frame_->Size().Height();

    center_offset_.SetPoint((width < client_size_.cx) ?
                                (client_size_.cx / 2) - (width / 2) : 0,
                            (height < client_size_.cy) ?
                                (client_size_.cy / 2) - (height / 2) : 0);

    CPoint scroll_pos(std::max(0L, std::min(scroll_pos_.x, width - client_size_.cx)),
                      std::max(0L, std::min(scroll_pos_.y, height - client_size_.cy)));

    ScrollWindowEx(scroll_pos_.x - scroll_pos.x,
                   scroll_pos_.y - scroll_pos.y,
                   nullptr, nullptr, nullptr, nullptr,
                   SW_INVALIDATE);

    scroll_pos_ = scroll_pos;

    UpdateScrollBars(width, height);
}

void UiVideoWindow::OnMouse(UINT msg, WPARAM wparam, LPARAM lparam)
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

    CPoint pos(lparam);

    DWORD flags = static_cast<DWORD>(wparam);

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

        if (!::ScreenToClient(hwnd(), &pos))
            return;
    }
    else
    {
        int scroll_delta_x = 0;
        int scroll_delta_y = 0;

        if (client_size_.cx < frame_->Size().Width())
        {
            if (pos.x > client_size_.cx - kScrollBorder)
                scroll_delta_x = kScrollDelta;
            else if (pos.x < kScrollBorder)
                scroll_delta_x = -kScrollDelta;
        }

        if (client_size_.cy < frame_->Size().Height())
        {
            if (pos.y > client_size_.cy - kScrollBorder)
                scroll_delta_y = kScrollDelta;
            else if (pos.y < kScrollBorder)
                scroll_delta_y = -kScrollDelta;
        }

        scroll_delta_.SetPoint(scroll_delta_x, scroll_delta_y);

        if (scroll_delta_x || scroll_delta_y)
            scroll_timer_.Start(hwnd(), kScrollTimerInterval);
        else
            scroll_timer_.Stop();
    }

    pos.Offset(-(center_offset_.x - std::abs(scroll_pos_.x)),
               -(center_offset_.y - std::abs(scroll_pos_.y)));

    if (frame_->Contains(pos.x, pos.y))
    {
        if (prev_pos_ != pos || mask != prev_mask_)
        {
            prev_pos_ = pos;
            prev_mask_ = mask & ~kWheelMask;

            if (mask & kWheelMask)
            {
                for (WORD i = 0; i < wheel_speed; ++i)
                {
                    delegate_->OnPointerEvent(DesktopPoint(pos.x, pos.y), mask);
                    delegate_->OnPointerEvent(DesktopPoint(pos.x, pos.y), mask & ~kWheelMask);
                }
            }
            else
            {
                delegate_->OnPointerEvent(DesktopPoint(pos.x, pos.y), mask);
            }
        }
    }
}

void UiVideoWindow::OnMouseLeave()
{
    has_mouse_ = false;
}

LRESULT UiVideoWindow::OnTimer(UINT_PTR event_id)
{
    switch (event_id)
    {
        case kScrollTimerId:
        {
            if (!has_mouse_ || !Scroll(scroll_delta_.x, scroll_delta_.y))
            {
                scroll_timer_.Stop();
            }
        }
        break;
    }

    return 0;
}

void UiVideoWindow::OnHScroll(UINT code, UINT pos)
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
            Scroll(client_size_.cx * -1 / 4, 0);
            break;

        case SB_PAGEDOWN:
            Scroll(client_size_.cx * 1 / 4, 0);
            break;

        case SB_THUMBPOSITION:
        case SB_THUMBTRACK:
            Scroll(pos - scroll_pos_.x, 0);
            break;
    }
}

void UiVideoWindow::OnVScroll(UINT code, UINT pos)
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
            Scroll(0, client_size_.cy * -1 / 4);
            break;

        case SB_PAGEDOWN:
            Scroll(0, client_size_.cy * 1 / 4);
            break;

        case SB_THUMBPOSITION:
        case SB_THUMBTRACK:
            Scroll(0, pos - scroll_pos_.y);
            break;
    }
}

void UiVideoWindow::UpdateScrollBars(int width, int height)
{
    int scroolbar_width = 0;
    int scroolbar_height = 0;

    if (client_size_.cx >= width)
        scroolbar_width = GetSystemMetrics(SM_CXHSCROLL);

    if (client_size_.cy >= height)
        scroolbar_height = GetSystemMetrics(SM_CYVSCROLL);

    SCROLLINFO si = { 0 };
    si.cbSize = sizeof(si);
    si.fMask  = SIF_PAGE | SIF_POS | SIF_RANGE;

    si.nMax  = width;
    si.nPage = client_size_.cx + scroolbar_width;
    si.nPos  = scroll_pos_.x;
    SetScrollInfo(SB_HORZ, &si, TRUE);

    si.nMax  = height;
    si.nPage = client_size_.cy + scroolbar_height;
    si.nPos  = scroll_pos_.y;
    SetScrollInfo(SB_VERT, &si, TRUE);

    Invalidate();
}

bool UiVideoWindow::Scroll(LONG delta_x, LONG delta_y)
{
    if (!frame_)
        return false;

    LONG offset_x = 0;
    LONG offset_y = 0;

    int width = frame_->Size().Width();
    int height = frame_->Size().Height();

    if (delta_x)
    {
        offset_x = std::max(delta_x, -scroll_pos_.x);
        offset_x = std::min(offset_x, width - client_size_.cx - scroll_pos_.x);
    }

    if (delta_y)
    {
        offset_y = std::max(delta_y, -scroll_pos_.y);
        offset_y = std::min(offset_y, height - client_size_.cy - scroll_pos_.y);
    }

    if (offset_x || offset_y)
    {
        scroll_pos_.Offset(offset_x, offset_y);

        ScrollWindowEx(-offset_x, -offset_y,
                       nullptr, nullptr, nullptr, nullptr,
                       SW_INVALIDATE);
        UpdateScrollBars(width, height);

        return true;
    }

    return false;
}

DesktopFrame* UiVideoWindow::Frame()
{
    return frame_.get();
}

void UiVideoWindow::DrawFrame()
{
    Invalidate();
}

DesktopSize UiVideoWindow::FrameSize()
{
    if (!frame_)
        return DesktopSize();

    return frame_->Size();
}

void UiVideoWindow::HasFocus(bool has)
{
    has_focus_ = has;
}

void UiVideoWindow::ResizeFrame(const DesktopSize& size, const PixelFormat& format)
{
    screen_dc_.Reset(CreateCompatibleDC(nullptr));
    memory_dc_.Reset(CreateCompatibleDC(screen_dc_));

    frame_ = DesktopFrameDIB::Create(size, format, memory_dc_);

    PostMessageW(WM_SIZE, 0, 0);
}

bool UiVideoWindow::OnMessage(UINT msg, WPARAM wparam, LPARAM lparam, LRESULT* result)
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
