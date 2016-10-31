/*
* PROJECT:         Aspia Remote Desktop
* FILE:            desktop_capture/screen_window_win.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "desktop_capture/screen_window_win.h"

#define WM_RESIZE_REMOTE_SCREEN   (WM_APP + 1)

ScreenWindowWin::ScreenWindowWin(HWND parent) :
    parent_(parent),
    window_(nullptr),
    image_buffer_(nullptr)
{
    scroll_pos_.x = 0;
    scroll_pos_.y = 0;

    scroll_max_.x = 0;
    scroll_max_.y = 0;

    center_offset_.x = 0;
    center_offset_.y = 0;
}

ScreenWindowWin::~ScreenWindowWin() {}

void ScreenWindowWin::AllocateBuffer(int align)
{
    DCHECK(screen_dc_);
    DCHECK(memory_dc_);

    int aligned_width = ((current_desktop_size_.width() + (align - 1)) / align) * 2;

    typedef struct
    {
        BITMAPINFOHEADER header;
        union
        {
            struct
            {
                uint32_t red;
                uint32_t green;
                uint32_t blue;
            } mask;
            RGBQUAD color[256];
        } u;
    } BITMAPINFOALT;

    BITMAPINFOALT bmi = { 0 };
    bmi.header.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.header.biBitCount    = current_pixel_format_.bits_per_pixel();
    bmi.header.biSizeImage   = current_pixel_format_.bytes_per_pixel() * aligned_width * current_desktop_size_.height();
    bmi.header.biPlanes      = 1;
    bmi.header.biWidth       = current_desktop_size_.width();
    bmi.header.biHeight      = -current_desktop_size_.height();
    bmi.header.biCompression = (current_pixel_format_.bits_per_pixel() == 8) ? BI_RGB : BI_BITFIELDS;

    if (current_pixel_format_.bits_per_pixel() == 8)
    {
        for (int i = 0; i < 256; ++i)
        {
            uint32_t red   = (i >> current_pixel_format_.red_shift())   & current_pixel_format_.red_max();
            uint32_t green = (i >> current_pixel_format_.green_shift()) & current_pixel_format_.green_max();
            uint32_t blue  = (i >> current_pixel_format_.blue_shift())  & current_pixel_format_.blue_max();

            bmi.u.color[i].rgbRed   = red   * 0xFF / current_pixel_format_.red_max();
            bmi.u.color[i].rgbGreen = green * 0xFF / current_pixel_format_.green_max();
            bmi.u.color[i].rgbBlue  = blue  * 0xFF / current_pixel_format_.blue_max();
        }
    }
    else
    {
        bmi.u.mask.red   = current_pixel_format_.red_max()   << current_pixel_format_.red_shift();
        bmi.u.mask.green = current_pixel_format_.green_max() << current_pixel_format_.green_shift();
        bmi.u.mask.blue  = current_pixel_format_.blue_max()  << current_pixel_format_.blue_shift();
    }

    target_bitmap_ = CreateDIBSection(memory_dc_,
                                      reinterpret_cast<PBITMAPINFO>(&bmi),
                                      DIB_RGB_COLORS,
                                      reinterpret_cast<void**>(&image_buffer_),
                                      nullptr,
                                      0);

    LOG(INFO) << "buffer allocated";
}

// static
void ScreenWindowWin::GetWindowSize(HWND window, SIZE &size)
{
    RECT rect;
    GetClientRect(window, &rect);

    size.cx = rect.right - rect.left;
    size.cy = rect.bottom - rect.top;
}

// static
void ScreenWindowWin::OnResize(HWND window)
{
    ScreenWindowWin *this_ =
        reinterpret_cast<ScreenWindowWin*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    SIZE window_size;
    GetWindowSize(window, window_size);

    UpdateScrollBars(this_, window_size);
}

ScreenWindowWin::LockedMemory ScreenWindowWin::GetBuffer()
{
    return LockedMemory(image_buffer_, &image_buffer_lock_);
}

void ScreenWindowWin::Resize(const DesktopSize &desktop_size,
                             const PixelFormat &pixel_format)
{
    if (current_desktop_size_ != desktop_size ||
        current_pixel_format_ != pixel_format)
    {
        LockGuard<Mutex> guard(&image_buffer_lock_);

        current_desktop_size_ = desktop_size;
        current_pixel_format_ = pixel_format;

        screen_dc_.set(CreateCompatibleDC(nullptr));
        memory_dc_.set(CreateCompatibleDC(screen_dc_));

        // Allocate 32 bit aligned image
        AllocateBuffer(32);

        scroll_max_.x = current_desktop_size_.width();
        scroll_max_.y = current_desktop_size_.height();

        PostMessageW(window_,
                     WM_RESIZE_REMOTE_SCREEN,
                     current_desktop_size_.width(),
                     current_desktop_size_.height());
    }
}

void ScreenWindowWin::Invalidate()
{
    InvalidateRect(window_, nullptr, FALSE);
}

// static
void ScreenWindowWin::FillNonScreenArea(ScreenWindowWin *this_, HDC hdc, RECT &paint_rect)
{
    HBRUSH black_brush = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));

    if (black_brush)
    {
        RECT l_rect = { paint_rect.left,
                        paint_rect.top,
                        abs(this_->center_offset_.x),
                        paint_rect.bottom };

        RECT r_rect = { abs(this_->center_offset_.x) + this_->current_desktop_size_.width(),
                        paint_rect.top,
                        paint_rect.right,
                        paint_rect.bottom };

        int l_rect_width = l_rect.right - l_rect.left;
        int r_rect_width = r_rect.right - r_rect.left;

        RECT t_rect = { paint_rect.left + l_rect_width,
                        paint_rect.top,
                        paint_rect.right - r_rect_width,
                        abs(this_->center_offset_.y) };

        RECT b_rect = { paint_rect.left + l_rect_width,
                        abs(this_->center_offset_.y) + this_->current_desktop_size_.height(),
                        paint_rect.right - r_rect_width,
                        paint_rect.bottom };

        FillRect(hdc, &l_rect, black_brush);
        FillRect(hdc, &r_rect, black_brush);
        FillRect(hdc, &t_rect, black_brush);
        FillRect(hdc, &b_rect, black_brush);
    }
}

// static
void ScreenWindowWin::OnPaint(HWND window)
{
    ScreenWindowWin *this_ =
        reinterpret_cast<ScreenWindowWin*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    if (!this_->screen_dc_)
        return;

    POINT scroll_pos;

    if (!this_->center_offset_.x)
        scroll_pos.x = this_->scroll_pos_.x;
    else
        scroll_pos.x = 0;

    if (!this_->center_offset_.y)
        scroll_pos.y = this_->scroll_pos_.y;
    else
        scroll_pos.y = 0;

    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(window, &ps);

    FillNonScreenArea(this_, hdc, ps.rcPaint);

    LockGuard<Mutex> guard(&this_->image_buffer_lock_);

    HGDIOBJ prev_object = SelectObject(this_->memory_dc_, this_->target_bitmap_);

    if (prev_object)
    {
        int width = ps.rcPaint.right - ps.rcPaint.left;
        int height = ps.rcPaint.bottom - ps.rcPaint.top;

        BitBlt(hdc,
               ps.rcPaint.left - this_->center_offset_.x,
               ps.rcPaint.top - this_->center_offset_.y,
               width - this_->center_offset_.x * 2,
               height - this_->center_offset_.y * 2,
               this_->memory_dc_,
               ps.rcPaint.left + scroll_pos.x,
               ps.rcPaint.top + scroll_pos.y,
               SRCCOPY);

        SelectObject(this_->memory_dc_, prev_object);
    }

    EndPaint(window, &ps);
}

// static
void ScreenWindowWin::OnCreate(HWND window, LPARAM lParam)
{
    ScreenWindowWin *this_ =
        reinterpret_cast<ScreenWindowWin*>(reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams);

    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this_));
}

// static
void ScreenWindowWin::OnDestroy(HWND window)
{

}

// static
void ScreenWindowWin::UpdateScrollBars(ScreenWindowWin *this_, SIZE &window_size)
{
    int width = 0;
    int height = 0;

    if (window_size.cx >= this_->current_desktop_size_.width())
        width = GetSystemMetrics(SM_CXHSCROLL);

    if (window_size.cy >= this_->current_desktop_size_.height())
        height = GetSystemMetrics(SM_CYVSCROLL);

    SCROLLINFO si = { 0 };
    si.cbSize = sizeof(si);
    si.fMask  = SIF_PAGE | SIF_POS | SIF_RANGE;
    si.nMin   = 0;

    si.nMax   = this_->scroll_max_.x;
    si.nPage  = window_size.cx + width;
    si.nPos   = this_->scroll_pos_.x;
    SetScrollInfo(this_->window_, SB_HORZ, &si, FALSE);

    si.nMax   = this_->scroll_max_.y;
    si.nPage  = window_size.cy + height;
    si.nPos   = this_->scroll_pos_.y;

    SetScrollInfo(this_->window_, SB_VERT, &si, TRUE);
}

// static
void ScreenWindowWin::Scroll(ScreenWindowWin *this_, POINT &delta, SIZE &window_size)
{
    if (delta.x)
    {
        delta.x = std::max(delta.x, -this_->scroll_pos_.x);
        delta.x = std::min(delta.x, this_->scroll_max_.x - window_size.cx - this_->scroll_pos_.x);
    }

    if (delta.y)
    {
        delta.y = std::max(delta.y, -this_->scroll_pos_.y);
        delta.y = std::min(delta.y, this_->scroll_max_.y - window_size.cy - this_->scroll_pos_.y);
    }

    if (delta.x || delta.y)
    {
        this_->scroll_pos_.x += delta.x;
        this_->scroll_pos_.y += delta.y;

        ScrollWindowEx(this_->window_,
                       -delta.x,
                       -delta.y,
                       nullptr,
                       nullptr,
                       nullptr,
                       nullptr,
                       SW_INVALIDATE);

        UpdateScrollBars(this_, window_size);
    }
}

// static
void ScreenWindowWin::OnHScroll(HWND window, WPARAM wParam)
{
    ScreenWindowWin *this_ =
        reinterpret_cast<ScreenWindowWin*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    SIZE window_size;
    GetWindowSize(window, window_size);

    POINT delta = { 0 };

    switch (LOWORD(wParam))
    {
        case SB_LINEUP:
            delta.x = -2;
            break;

        case SB_LINEDOWN:
            delta.x = 2;
            break;

        case SB_PAGEUP:
            delta.x = window_size.cx * -1 / 4;
            break;

        case SB_PAGEDOWN:
            delta.x = window_size.cy * 1 / 4;
            break;

        case SB_THUMBPOSITION:
            // high word contains current position
            delta.x = HIWORD(wParam) - this_->scroll_pos_.x;
            break;

        case SB_THUMBTRACK:
            // high word contains current position
            delta.x = HIWORD(wParam) - this_->scroll_pos_.x;
            break;
    }

    Scroll(this_, delta, window_size);
}

// static
void ScreenWindowWin::OnVScroll(HWND window, WPARAM wParam)
{
    ScreenWindowWin *this_ =
        reinterpret_cast<ScreenWindowWin*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    SIZE window_size;
    GetWindowSize(window, window_size);

    POINT delta = { 0 };

    switch (LOWORD(wParam))
    {
        case SB_LINEUP:
            delta.y = -2;
            break;

        case SB_LINEDOWN:
            delta.y = 2;
            break;

        case SB_PAGEUP:
            delta.y = window_size.cy * -1 / 4;
            break;

        case SB_PAGEDOWN:
            delta.y = window_size.cy * 1 / 4;
            break;

        case SB_THUMBPOSITION:
            // high word contains current position
            delta.y = HIWORD(wParam) - this_->scroll_pos_.y;
            break;

        case SB_THUMBTRACK:
            // high word contains current position
            delta.y = HIWORD(wParam) - this_->scroll_pos_.y;
            break;
    }

    Scroll(this_, delta, window_size);
}

// static
void ScreenWindowWin::OnSize(HWND window)
{
    ScreenWindowWin *this_ =
        reinterpret_cast<ScreenWindowWin*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    SIZE window_size;
    GetWindowSize(window, window_size);

    if (this_->current_desktop_size_.width() < window_size.cx)
        this_->center_offset_.x = (this_->current_desktop_size_.width() / 2) - (window_size.cx / 2);
    else
        this_->center_offset_.x = 0;

    if (this_->current_desktop_size_.height() < window_size.cy)
        this_->center_offset_.y = (this_->current_desktop_size_.height() / 2) - (window_size.cy / 2);
    else
        this_->center_offset_.y = 0;

    POINT new_scroll_pos;

    new_scroll_pos.x = std::max(0L, std::min(this_->scroll_pos_.x, this_->scroll_max_.x - std::max(window_size.cx, 0L)));
    new_scroll_pos.y = std::max(0L, std::min(this_->scroll_pos_.y, this_->scroll_max_.y - std::max(window_size.cy, 0L)));

    ScrollWindowEx(window,
                   this_->scroll_pos_.x - new_scroll_pos.x,
                   this_->scroll_pos_.y - new_scroll_pos.y,
                   nullptr,
                   nullptr,
                   nullptr,
                   nullptr,
                   SW_INVALIDATE);

    this_->scroll_pos_ = new_scroll_pos;

    UpdateScrollBars(this_, window_size);
}

// static
void ScreenWindowWin::OnClose(HWND window)
{
    ScreenWindowWin *this_ =
        reinterpret_cast<ScreenWindowWin*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    this_->Stop();
}

// static
LRESULT CALLBACK ScreenWindowWin::WndProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_PAINT:
            OnPaint(window);
            break;

        case WM_RESIZE_REMOTE_SCREEN:
            OnResize(window);
            break;

        case WM_LBUTTONUP:
        case WM_LBUTTONDOWN:
        case WM_MOUSEMOVE:
        case WM_MBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_RBUTTONDOWN:
            break;

        case WM_SIZE:
        case WM_WINDOWPOSCHANGED:
            OnSize(window);
            break;

        case WM_HSCROLL:
            OnHScroll(window, wParam);
            break;

        case WM_VSCROLL:
            OnVScroll(window, wParam);
            break;

        case WM_CREATE:
            OnCreate(window, lParam);
            break;

        case WM_DESTROY:
            OnDestroy(window);
            break;

        case WM_CLOSE:
            OnClose(window);
            break;
    }

    return DefWindowProcW(window, msg, wParam, lParam);
}

void ScreenWindowWin::Worker()
{
    WCHAR class_name[] = L"Aspia.Video.Window.Class";

    WNDCLASSEXW window_class = { 0 };

    window_class.cbSize        = sizeof(window_class);
    window_class.style         = CS_BYTEALIGNWINDOW;
    window_class.lpfnWndProc   = WndProc;
    window_class.lpszClassName = class_name;

    ATOM atom = RegisterClassExW(&window_class);
    DCHECK(atom) << "Unable to create screen window class: " << GetLastError();

    DWORD style = parent_ ? WS_CHILD : WS_OVERLAPPEDWINDOW;

    window_ = CreateWindowExW(0,
                              class_name,
                              L"Video Window",
                              style,
                              0, 0, CW_USEDEFAULT, CW_USEDEFAULT,
                              parent_,
                              nullptr,
                              nullptr,
                              this);
    DCHECK(window_) << "Unable to create screen window: " << GetLastError();

    ShowWindow(window_, SW_SHOW);
    UpdateWindow(window_);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnregisterClassW(class_name, nullptr);
}

void ScreenWindowWin::OnStart() {}

void ScreenWindowWin::OnStop()
{
    DestroyWindow(window_);
}
