//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/splitter.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/base/splitter.h"
#include "ui/base/module.h"
#include "base/scoped_gdi_object.h"
#include "base/scoped_select_object.h"

#include <commctrl.h>

namespace aspia {

static const int kSplitPanelWidth = 6;

bool Splitter::CreateWithFixedLeft(HWND parent, int position)
{
    fixed_left_ = true;
    x_ = position;
    return CreateInternal(parent);
}

bool Splitter::CreateWithProportion(HWND parent)
{
    fixed_left_ = false;
    return CreateInternal(parent);
}

bool Splitter::CreateInternal(HWND parent)
{
    cursor_ = LoadCursorW(nullptr, IDC_SIZEWE);
    return Create(parent, WS_CHILD | WS_VISIBLE);
}

void Splitter::SetPanels(HWND left, HWND right)
{
    left_panel_.Attach(left);
    right_panel_.Attach(right);
}

void Splitter::OnCreate()
{
    split_panel_.Attach(CreateWindowExW(0,
                                        WC_STATICW,
                                        nullptr,
                                        WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
                                        x_, 0, kSplitPanelWidth, 0,
                                        hwnd(),
                                        nullptr,
                                        Module().Current().Handle(),
                                        nullptr));
}

void Splitter::OnDestroy()
{
    split_panel_.DestroyWindow();
}

void Splitter::OnSize(const DesktopSize& size)
{
    HDWP dwp = BeginDeferWindowPos(3);

    if (dwp)
    {
        height_ = size.Height();

        if (!fixed_left_)
        {
            // Proportional mode.

            if (prev_size_.IsEmpty())
            {
                x_ = (size.Width() / 2) - (kSplitPanelWidth / 2);
                prev_size_ = size;
            }

            int prev_x = (prev_size_.Width() / 2) - (kSplitPanelWidth / 2);
            int curr_x = (size.Width() / 2) - (kSplitPanelWidth / 2);

            x_ += curr_x - prev_x;
        }

        x_ = NormalizePosition(x_);

        DeferWindowPos(dwp,
                       left_panel_,
                       nullptr,
                       0, 0,
                       x_, height_,
                       SWP_NOACTIVATE | SWP_NOZORDER);

        DeferWindowPos(dwp,
                       right_panel_,
                       nullptr,
                       x_ + kSplitPanelWidth, 0,
                       size.Width() - x_ - kSplitPanelWidth, height_,
                       SWP_NOACTIVATE | SWP_NOZORDER);

        DeferWindowPos(dwp,
                       split_panel_,
                       nullptr,
                       x_, 0,
                       kSplitPanelWidth, height_,
                       SWP_NOACTIVATE | SWP_NOZORDER);

        EndDeferWindowPos(dwp);
    }

    prev_size_ = size;
}

void Splitter::Draw()
{
    HDC hdc = GetWindowDC(hwnd());
    if (hdc)
    {
        static const WORD kDotPattern[8] =
            { 0x00AA, 0x0055, 0x00AA, 0x0055,
              0x00AA, 0x0055, 0x00AA, 0x0055 };

        {
            ScopedHBITMAP bitmap(CreateBitmap(8, 8, 1, 1, kDotPattern));

            {
                ScopedHBRUSH brush(CreatePatternBrush(bitmap));

                {
                    ScopedSelectObject(hdc, brush);
                    PatBlt(hdc, x_, 0, kSplitPanelWidth, height_, PATINVERT);
                }
            }
        }

        ReleaseDC(hwnd(), hdc);
    }
}

int Splitter::NormalizePosition(int position)
{
    int width = ClientWidth();

    if (position > (width - kSplitPanelWidth))
        return width - kSplitPanelWidth;
    else if (position < 0)
        return 0;

    return position;
}

void Splitter::OnLButtonDown()
{
    is_sizing_ = true;
    x_ = NormalizePosition(CursorPositionInWindow().x());

    SetCapture(hwnd());
    Draw();
}

void Splitter::OnLButtonUp()
{
    if (is_sizing_)
    {
        is_sizing_ = false;

        Draw();
        OnSize(ClientSize());
        ReleaseCapture();
    }
}

void Splitter::OnMouseMove()
{
    ::SetCursor(cursor_);

    if (!is_sizing_)
        return;

    Draw();
    x_ = NormalizePosition(CursorPositionInWindow().x());
    Draw();
}

void Splitter::OnDrawItem(LPDRAWITEMSTRUCT dis)
{
    if (dis->hwndItem == split_panel_.hwnd())
    {
        int saved_dc = SaveDC(dis->hDC);

        if (saved_dc)
        {
            FillRect(dis->hDC, &dis->rcItem, GetSysColorBrush(COLOR_WINDOW));

            HBRUSH border_brush = GetSysColorBrush(COLOR_WINDOWFRAME);

            if (border_brush)
            {
                dis->rcItem.left = 0;
                dis->rcItem.right = dis->rcItem.left + 1;

                FillRect(dis->hDC, &dis->rcItem, border_brush);

                dis->rcItem.left = kSplitPanelWidth - 1;
                dis->rcItem.right = dis->rcItem.left + 1;

                FillRect(dis->hDC, &dis->rcItem, border_brush);
            }

            RestoreDC(dis->hDC, saved_dc);
        }
    }
}

bool Splitter::OnMessage(UINT msg, WPARAM wparam, LPARAM lparam, LRESULT* result)
{
    switch (msg)
    {
        case WM_CREATE:
            OnCreate();
            break;

        case WM_DESTROY:
            OnDestroy();
            break;

        case WM_SIZE:
            OnSize(DesktopSize(LOWORD(lparam), HIWORD(lparam)));
            break;

        case WM_LBUTTONUP:
            OnLButtonUp();
            break;

        case WM_LBUTTONDOWN:
            OnLButtonDown();
            break;

        case WM_MOUSEMOVE:
            OnMouseMove();
            break;

        case WM_DRAWITEM:
            OnDrawItem(reinterpret_cast<LPDRAWITEMSTRUCT>(lparam));
            break;

        default:
            return false;
    }

    *result = 0;
    return true;
}

} // namespace aspia
