//
// PROJECT:         Aspia Remote Desktop
// FILE:            gui/video_window.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "gui/video_window.h"

#include <algorithm>

#include "proto/proto.pb.h"

namespace aspia {

static const UINT_PTR kScrollTimerId = 1;
static const int kScrollDelta = 25;

static const uint8_t kWheelMask = proto::PointerEvent::WHEEL_DOWN | proto::PointerEvent::WHEEL_UP;

VideoWindow::VideoWindow(Client *client) :
    client_(client),
    background_brush_(CreateSolidBrush(RGB(25, 25, 25))),
    scroll_timer_(kScrollTimerId),
    has_mouse_(false),
    has_focus_(false),
    prev_mask_(0)
{
    // Nothing
}

VideoWindow::~VideoWindow()
{
    // Nothing
}

LRESULT VideoWindow::OnCreate(LPCREATESTRUCT create_struct)
{
    return 0;
}

void VideoWindow::DrawBackground(CPaintDC &target_dc)
{
    CRect left(target_dc.m_ps.rcPaint.left,
               target_dc.m_ps.rcPaint.top,
               center_offset_.x,
               target_dc.m_ps.rcPaint.bottom);

    CRect right(center_offset_.x + buffer_.Size().Width(),
                target_dc.m_ps.rcPaint.top,
                target_dc.m_ps.rcPaint.right,
                target_dc.m_ps.rcPaint.bottom);

    CRect top(target_dc.m_ps.rcPaint.left + left.Width(),
              target_dc.m_ps.rcPaint.top,
              target_dc.m_ps.rcPaint.right - right.Width(),
              center_offset_.y);

    CRect bottom(target_dc.m_ps.rcPaint.left + left.Width(),
                 center_offset_.y + buffer_.Size().Height(),
                 target_dc.m_ps.rcPaint.right - right.Width(),
                 target_dc.m_ps.rcPaint.bottom);

    target_dc.FillRect(&left, background_brush_);
    target_dc.FillRect(&right, background_brush_);
    target_dc.FillRect(&top, background_brush_);
    target_dc.FillRect(&bottom, background_brush_);
}

void VideoWindow::OnPaint(CDCHandle dc)
{
    CPaintDC target_dc(*this);

    if (screen_dc_)
    {
        CPoint scroll_pos(center_offset_.x ? 0 : scroll_pos_.x,
                          center_offset_.y ? 0 : scroll_pos_.y);

        DrawBackground(target_dc);

        CRect paint(target_dc.m_ps.rcPaint);

        {
            LockGuard<Lock> guard(&buffer_lock_);

            buffer_.DrawTo(target_dc,
                           // Координаты буфера в которые будет произведено рисование.
                           DesktopPoint(paint.left + center_offset_.x, paint.top + center_offset_.y),
                           // Координаты буфера из которых будет произведено рисование.
                           DesktopPoint(paint.left + scroll_pos.x, paint.top + scroll_pos.y),
                           // Размеры буфера (ширина и высота).
                           DesktopSize(paint.Width() - (center_offset_.x * 2), paint.Height() - (center_offset_.y * 2)));
        }
    }
    else
    {
        target_dc.FillRect(&target_dc.m_ps.rcPaint, background_brush_);
    }
}

BOOL VideoWindow::OnEraseBackground(CDCHandle dc)
{
    return FALSE;
}

void VideoWindow::OnWindowPosChanged(LPWINDOWPOS pos)
{
    OnSize(SIZE_RESTORED, CSize());
}

void VideoWindow::OnSize(UINT type, CSize size)
{
    CRect rect;
    GetClientRect(&rect);

    client_size_ = rect.Size();

    int32_t width = buffer_.Size().Width();
    int32_t height = buffer_.Size().Height();

    if (width < client_size_.cx)
        center_offset_.x = (client_size_.cx / 2) - (width / 2);
    else
        center_offset_.x = 0;

    if (height < client_size_.cy)
        center_offset_.y = (client_size_.cy / 2) - (height / 2);
    else
        center_offset_.y = 0;

    CPoint scroll_pos;
    scroll_pos.x = std::max(0L, std::min(scroll_pos_.x, width - client_size_.cx));
    scroll_pos.y = std::max(0L, std::min(scroll_pos_.y, height - client_size_.cy));

    ScrollWindowEx(scroll_pos_.x - scroll_pos.x,
                   scroll_pos_.y - scroll_pos.y,
                   SW_INVALIDATE);

    scroll_pos_ = scroll_pos;

    UpdateScrollBars();
}

LRESULT VideoWindow::OnMouse(UINT msg, WPARAM wParam, LPARAM lParam, BOOL &handled)
{
    if (!has_focus_) return 0;

    if (!has_mouse_)
    {
        TRACKMOUSEEVENT tme;

        tme.cbSize = sizeof(TRACKMOUSEEVENT);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = *this;

        TrackMouseEvent(&tme);

        has_mouse_ = true;
    }

    CPoint pos(lParam);

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

        if (!ScreenToClient(&pos))
            return 0;
    }
    else
    {
        scroll_delta_.SetPoint(0, 0);

        if (client_size_.cx < buffer_.Size().Width())
        {
            if (pos.x > client_size_.cx - 15)
                scroll_delta_.x = kScrollDelta;
            else if (pos.x < 15)
                scroll_delta_.x = -kScrollDelta;
        }

        if (client_size_.cy < buffer_.Size().Height())
        {
            if (pos.y > client_size_.cy - 15)
                scroll_delta_.y = kScrollDelta;
            else if (pos.y < 15)
                scroll_delta_.y = -kScrollDelta;
        }

        if (scroll_delta_.x || scroll_delta_.y)
            scroll_timer_.Start(*this);
        else
            scroll_timer_.Stop();
    }

    pos.x -= center_offset_.x - std::abs(scroll_pos_.x);
    pos.y -= center_offset_.y - std::abs(scroll_pos_.y);

    if (buffer_.Contains(pos.x, pos.y))
    {
        if (pos != prev_pos_ || mask != prev_mask_)
        {
            prev_pos_ = pos;
            prev_mask_ = mask & ~kWheelMask;

            if (mask & kWheelMask)
            {
                for (WORD i = 0; i < wheel_speed; ++i)
                {
                    client_->SendPointerEvent(pos.x, pos.y, mask);
                    client_->SendPointerEvent(pos.x, pos.y, mask & ~kWheelMask);
                }
            }
            else
            {
                client_->SendPointerEvent(pos.x, pos.y, mask);
            }
        }
    }

    return 0;
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
            if (!has_mouse_ || !Scroll(scroll_delta_))
            {
                scroll_timer_.Stop();
            }
        }
        break;
    }

    return 0;
}

void VideoWindow::OnHScroll(UINT code, UINT pos, CScrollBar scrollbar)
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

void VideoWindow::OnVScroll(UINT code, UINT pos, CScrollBar scrollbar)
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

void VideoWindow::UpdateScrollBars()
{
    CSize scrollbar;

    int width = buffer_.Size().Width();
    int height = buffer_.Size().Height();

    if (client_size_.cx >= width)
        scrollbar.cx = GetSystemMetrics(SM_CXHSCROLL);

    if (client_size_.cy >= height)
        scrollbar.cy = GetSystemMetrics(SM_CYVSCROLL);

    SCROLLINFO si = { 0 };
    si.cbSize = sizeof(si);
    si.fMask  = SIF_PAGE | SIF_POS | SIF_RANGE;

    si.nMax  = width;
    si.nPage = client_size_.cx + scrollbar.cx;
    si.nPos  = scroll_pos_.x;
    SetScrollInfo(SB_HORZ, &si);

    si.nMax  = height;
    si.nPage = client_size_.cy + scrollbar.cy;
    si.nPos  = scroll_pos_.y;
    SetScrollInfo(SB_VERT, &si);

    Invalidate(FALSE);
}

bool VideoWindow::Scroll(LONG delta_x, LONG delta_y)
{
    CPoint offset;

    if (delta_x)
    {
        offset.x = std::max(delta_x, -scroll_pos_.x);
        offset.x = std::min(offset.x, buffer_.Size().Width() - client_size_.cx - scroll_pos_.x);
    }

    if (delta_y)
    {
        offset.y = std::max(delta_y, -scroll_pos_.y);
        offset.y = std::min(offset.y, buffer_.Size().Height() - client_size_.cy - scroll_pos_.y);
    }

    if (offset.x || offset.y)
    {
        scroll_pos_.Offset(offset);

        ScrollWindowEx(-offset.x, -offset.y, SW_INVALIDATE);
        UpdateScrollBars();

        return true;
    }

    return false;
}

bool VideoWindow::Scroll(const CPoint &delta)
{
    return Scroll(delta.x, delta.y);
}

void VideoWindow::OnVideoUpdate(const uint8_t *buffer, const DesktopRegion &region)
{
    buffer_.CopyFrom(buffer, region);

    Invalidate(FALSE);
}

void VideoWindow::OnVideoResize(const DesktopSize &size, const PixelFormat &format)
{
    {
        LockGuard<Lock> guard(&buffer_lock_);

        screen_dc_.set(CreateCompatibleDC(nullptr));
        buffer_.Resize(screen_dc_, size, format);
    }

    PostMessageW(WM_SIZE);
}

} // namespace aspia
