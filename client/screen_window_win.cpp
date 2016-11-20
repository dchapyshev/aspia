/*
* PROJECT:         Aspia Remote Desktop
* FILE:            client/screen_window_win.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "client/screen_window_win.h"

ScreenWindowWin::ScreenWindowWin(HWND parent,
                                 OnMessageAvailableCallback on_message,
                                 OnClosedCallback on_closed) :
    parent_(parent),
    on_message_(on_message),
    on_closed_(on_closed),
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

ScreenWindowWin::~ScreenWindowWin()
{
    // Nothing
}

void ScreenWindowWin::AllocateBuffer(int align)
{
    DCHECK(screen_dc_);
    DCHECK(memory_dc_);

    int aligned_width = ((screen_size_.width() + (align - 1)) / align) * 2;

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
    bmi.header.biBitCount    = pixel_format_.bits_per_pixel();
    bmi.header.biSizeImage   = pixel_format_.bytes_per_pixel() * aligned_width * screen_size_.height();
    bmi.header.biPlanes      = 1;
    bmi.header.biWidth       = screen_size_.width();
    bmi.header.biHeight      = -screen_size_.height();
    bmi.header.biCompression = (pixel_format_.bits_per_pixel() == 8) ? BI_RGB : BI_BITFIELDS;

    if (pixel_format_.bits_per_pixel() == 8)
    {
        for (int i = 0; i < 256; ++i)
        {
            uint32_t red   = (i >> pixel_format_.red_shift())   & pixel_format_.red_max();
            uint32_t green = (i >> pixel_format_.green_shift()) & pixel_format_.green_max();
            uint32_t blue  = (i >> pixel_format_.blue_shift())  & pixel_format_.blue_max();

            bmi.u.color[i].rgbRed   = red   * 0xFF / pixel_format_.red_max();
            bmi.u.color[i].rgbGreen = green * 0xFF / pixel_format_.green_max();
            bmi.u.color[i].rgbBlue  = blue  * 0xFF / pixel_format_.blue_max();
        }
    }
    else
    {
        bmi.u.mask.red   = pixel_format_.red_max()   << pixel_format_.red_shift();
        bmi.u.mask.green = pixel_format_.green_max() << pixel_format_.green_shift();
        bmi.u.mask.blue  = pixel_format_.blue_max()  << pixel_format_.blue_shift();
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

ScreenWindowWin::LockedMemory ScreenWindowWin::GetBuffer()
{
    return LockedMemory(image_buffer_, &image_buffer_lock_);
}

void ScreenWindowWin::Resize(const DesktopSize &screen_size,
                             const PixelFormat &pixel_format)
{
    {
        LockGuard<Mutex> guard(&image_buffer_lock_);

        screen_size_ = screen_size;
        pixel_format_ = pixel_format;

        screen_dc_.set(CreateCompatibleDC(nullptr));
        memory_dc_.set(CreateCompatibleDC(screen_dc_));

        // Allocate 32 bit aligned image
        AllocateBuffer(32);

        scroll_max_.x = screen_size_.width();
        scroll_max_.y = screen_size_.height();
    }

    PostMessage(window_, WM_SIZE, 0, 0);
}

void ScreenWindowWin::Invalidate()
{
    InvalidateRect(window_, nullptr, FALSE);
}

// static
void ScreenWindowWin::FillNonScreenArea(ScreenWindowWin *self, HDC hdc, RECT &paint_rect)
{
    HBRUSH black_brush = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));

    if (black_brush)
    {
        RECT l_rect = { paint_rect.left,
                        paint_rect.top,
                        abs(self->center_offset_.x),
                        paint_rect.bottom };

        RECT r_rect = { abs(self->center_offset_.x) + self->screen_size_.width(),
                        paint_rect.top,
                        paint_rect.right,
                        paint_rect.bottom };

        int l_rect_width = l_rect.right - l_rect.left;
        int r_rect_width = r_rect.right - r_rect.left;

        RECT t_rect = { paint_rect.left + l_rect_width,
                        paint_rect.top,
                        paint_rect.right - r_rect_width,
                        abs(self->center_offset_.y) };

        RECT b_rect = { paint_rect.left + l_rect_width,
                        abs(self->center_offset_.y) + self->screen_size_.height(),
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
    ScreenWindowWin *self =
        reinterpret_cast<ScreenWindowWin*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    if (!self->screen_dc_)
        return;

    POINT scroll_pos = { 0 };

    if (!self->center_offset_.x)
        scroll_pos.x = self->scroll_pos_.x;

    if (!self->center_offset_.y)
        scroll_pos.y = self->scroll_pos_.y;

    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(window, &ps);

    FillNonScreenArea(self, hdc, ps.rcPaint);

    {
        LockGuard<Mutex> guard(&self->image_buffer_lock_);

        HGDIOBJ prev_object = SelectObject(self->memory_dc_, self->target_bitmap_);

        if (prev_object)
        {
            int width = ps.rcPaint.right - ps.rcPaint.left;
            int height = ps.rcPaint.bottom - ps.rcPaint.top;

            BitBlt(hdc,
                   ps.rcPaint.left - self->center_offset_.x,
                   ps.rcPaint.top - self->center_offset_.y,
                   width - self->center_offset_.x * 2,
                   height - self->center_offset_.y * 2,
                   self->memory_dc_,
                   ps.rcPaint.left + scroll_pos.x,
                   ps.rcPaint.top + scroll_pos.y,
                   SRCCOPY);

            SelectObject(self->memory_dc_, prev_object);
        }
    }

    EndPaint(window, &ps);
}

// static
void ScreenWindowWin::OnCreate(HWND window, LPARAM lParam)
{
    ScreenWindowWin *self =
        reinterpret_cast<ScreenWindowWin*>(reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams);

    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
}

// static
void ScreenWindowWin::OnDestroy(HWND window)
{
    PostQuitMessage(0);
}

// static
void ScreenWindowWin::UpdateScrollBars(ScreenWindowWin *self, SIZE &window_size)
{
    int width = 0;
    int height = 0;

    if (window_size.cx >= self->screen_size_.width())
        width = GetSystemMetrics(SM_CXHSCROLL);

    if (window_size.cy >= self->screen_size_.height())
        height = GetSystemMetrics(SM_CYVSCROLL);

    SCROLLINFO si = { 0 };
    si.cbSize = sizeof(si);
    si.fMask  = SIF_PAGE | SIF_POS | SIF_RANGE;
    si.nMin   = 0;

    si.nMax   = self->scroll_max_.x;
    si.nPage  = window_size.cx + width;
    si.nPos   = self->scroll_pos_.x;
    SetScrollInfo(self->window_, SB_HORZ, &si, FALSE);

    si.nMax   = self->scroll_max_.y;
    si.nPage  = window_size.cy + height;
    si.nPos   = self->scroll_pos_.y;

    SetScrollInfo(self->window_, SB_VERT, &si, TRUE);
}

// static
void ScreenWindowWin::Scroll(ScreenWindowWin *self, POINT &delta, SIZE &window_size)
{
    if (delta.x)
    {
        delta.x = std::max(delta.x, -self->scroll_pos_.x);
        delta.x = std::min(delta.x, self->scroll_max_.x - window_size.cx - self->scroll_pos_.x);
    }

    if (delta.y)
    {
        delta.y = std::max(delta.y, -self->scroll_pos_.y);
        delta.y = std::min(delta.y, self->scroll_max_.y - window_size.cy - self->scroll_pos_.y);
    }

    if (delta.x || delta.y)
    {
        self->scroll_pos_.x += delta.x;
        self->scroll_pos_.y += delta.y;

        ScrollWindowEx(self->window_,
                       -delta.x,
                       -delta.y,
                       nullptr,
                       nullptr,
                       nullptr,
                       nullptr,
                       SW_INVALIDATE);

        UpdateScrollBars(self, window_size);
    }
}

// static
void ScreenWindowWin::OnHScroll(HWND window, WPARAM wParam)
{
    ScreenWindowWin *self =
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
            delta.x = HIWORD(wParam) - self->scroll_pos_.x;
            break;

        case SB_THUMBTRACK:
            // high word contains current position
            delta.x = HIWORD(wParam) - self->scroll_pos_.x;
            break;
    }

    Scroll(self, delta, window_size);
}

// static
void ScreenWindowWin::OnVScroll(HWND window, WPARAM wParam)
{
    ScreenWindowWin *self =
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
            delta.y = HIWORD(wParam) - self->scroll_pos_.y;
            break;

        case SB_THUMBTRACK:
            // high word contains current position
            delta.y = HIWORD(wParam) - self->scroll_pos_.y;
            break;
    }

    Scroll(self, delta, window_size);
}

// static
void ScreenWindowWin::OnSize(HWND window)
{
    ScreenWindowWin *self =
        reinterpret_cast<ScreenWindowWin*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    SIZE window_size;
    GetWindowSize(window, window_size);

    if (self->screen_size_.width() < window_size.cx)
        self->center_offset_.x = (self->screen_size_.width() / 2) - (window_size.cx / 2);
    else
        self->center_offset_.x = 0;

    if (self->screen_size_.height() < window_size.cy)
        self->center_offset_.y = (self->screen_size_.height() / 2) - (window_size.cy / 2);
    else
        self->center_offset_.y = 0;

    POINT new_scroll_pos;

    new_scroll_pos.x = std::max(0L, std::min(self->scroll_pos_.x, self->scroll_max_.x - std::max(window_size.cx, 0L)));
    new_scroll_pos.y = std::max(0L, std::min(self->scroll_pos_.y, self->scroll_max_.y - std::max(window_size.cy, 0L)));

    ScrollWindowEx(window,
                   self->scroll_pos_.x - new_scroll_pos.x,
                   self->scroll_pos_.y - new_scroll_pos.y,
                   nullptr,
                   nullptr,
                   nullptr,
                   nullptr,
                   SW_INVALIDATE);

    self->scroll_pos_ = new_scroll_pos;

    UpdateScrollBars(self, window_size);

    InvalidateRect(window, nullptr, FALSE);
}

// static
void ScreenWindowWin::OnClose(HWND window)
{
    ScreenWindowWin *self =
        reinterpret_cast<ScreenWindowWin*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    self->Stop();
}

void ScreenWindowWin::SendPointerEvent(int32_t x, int32_t y, int32_t mask)
{
    std::unique_ptr<proto::ClientToServer> message(new proto::ClientToServer());

    proto::PointerEvent *pe = message->mutable_pointer_event();

    pe->set_x(x);
    pe->set_y(y);
    pe->set_mask(mask);

    on_message_(message);
}

void ScreenWindowWin::SendKeyEvent(int32_t keycode, bool extended, bool pressed)
{
    std::unique_ptr<proto::ClientToServer> message(new proto::ClientToServer());

    proto::KeyEvent *ke = message->mutable_key_event();

    ke->set_keycode(keycode);
    ke->set_extended(extended);
    ke->set_pressed(pressed);

    on_message_(message);
}

// static
void ScreenWindowWin::OnMouseMessage(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    ScreenWindowWin *self =
        reinterpret_cast<ScreenWindowWin*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    POINT pos;
    pos.x = LOWORD(lParam);
    pos.y = HIWORD(lParam);

    uint32_t flags = static_cast<uint32_t>(wParam);

    uint8_t mask = ((flags & MK_LBUTTON) ? proto::PointerEvent::LEFT_BUTTON : 0) |
                   ((flags & MK_MBUTTON) ? proto::PointerEvent::MIDDLE_BUTTON : 0) |
                   ((flags & MK_RBUTTON) ? proto::PointerEvent::RIGHT_BUTTON : 0);

    uint8_t wheel_mask = 0;

    if (msg == WM_MOUSEWHEEL)
    {
        signed short speed = static_cast<signed short>(HIWORD(wParam));

        if (speed < 0)
        {
            mask |= proto::PointerEvent::WHEEL_DOWN;
        }
        else
        {
            mask |= proto::PointerEvent::WHEEL_UP;
        }

        if (!ScreenToClient(window, &pos))
        {
            pos.x = 0;
            pos.y = 0;
        }

        wheel_mask = proto::PointerEvent::WHEEL_DOWN | proto::PointerEvent::WHEEL_UP;
    }

    pos.x -= std::abs(self->center_offset_.x) - std::abs(self->scroll_pos_.x);
    pos.y -= std::abs(self->center_offset_.y) - std::abs(self->scroll_pos_.y);

    if (pos.x < 0 || pos.x > self->screen_size_.width() ||
        pos.y < 0 || pos.y > self->screen_size_.height())
    {
        return;
    }

    self->SendPointerEvent(pos.x, pos.y, mask);

    if (mask & wheel_mask)
    {
        self->SendPointerEvent(pos.x, pos.y, mask & ~wheel_mask);
    }
}

void ScreenWindowWin::SendCAD()
{
    SendKeyEvent(VK_CONTROL, false, true);
    SendKeyEvent(VK_MENU, false, true);
    SendKeyEvent(VK_DELETE, true, true);

    SendKeyEvent(VK_CONTROL, false, false);
    SendKeyEvent(VK_MENU, false, false);
    SendKeyEvent(VK_DELETE, true, false);
}

void ScreenWindowWin::OnKeyMessage(HWND window, WPARAM wParam, LPARAM lParam)
{
    ScreenWindowWin *self =
        reinterpret_cast<ScreenWindowWin*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    uint32_t key_data = static_cast<uint32_t>(lParam);

    bool pressed = ((key_data & 0x80000000) == 0);
    bool extended = ((key_data & 0x1000000) != 0);

    uint8_t key = static_cast<uint8_t>(static_cast<uint32_t>(wParam) & 255);

    self->SendKeyEvent(key, extended, pressed);
}

// static
LRESULT CALLBACK ScreenWindowWin::WndProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_PAINT:
            OnPaint(window);
            break;

        case WM_MOUSEWHEEL:
        case WM_LBUTTONUP:
        case WM_LBUTTONDOWN:
        case WM_MOUSEMOVE:
        case WM_MBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_RBUTTONDOWN:
            OnMouseMessage(window, msg, wParam, lParam);
            return 0;

        case WM_SYSKEYUP:
        case WM_KEYUP:
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            OnKeyMessage(window, wParam, lParam);
            return 0;

        case WM_CHAR:
        case WM_SYSCHAR:
        case WM_DEADCHAR:
        case WM_SYSDEADCHAR:
            return 0;

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

static LRESULT CALLBACK KeyboardHookProc(INT code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION)
    {
        GUITHREADINFO gui_info = { 0 };

        gui_info.cbSize = sizeof(GUITHREADINFO);

        if (GetGUIThreadInfo(0, &gui_info) && gui_info.hwndFocus)
        {
            PKBDLLHOOKSTRUCT hook_struct = reinterpret_cast<PKBDLLHOOKSTRUCT>(lParam);

            switch (hook_struct->vkCode)
            {
                case VK_TAB:
                case VK_LWIN:
                case VK_RWIN:
                {
                    // Если клавиша TAB нажата, а ALT не нажата.
                    if (hook_struct->vkCode == VK_TAB && !(hook_struct->flags & LLKHF_ALTDOWN))
                        break;

                    DWORD flags = 0;

                    if (hook_struct->flags & LLKHF_EXTENDED)
                    {
                        // Бит 24 установленный в 1 означает, что это расширенный код клавиши.
                        flags |= 0x1000000;
                    }

                    if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
                    {
                        // Бит 31 установленный в 1 означает, что клавиша отжата.
                        flags |= 0x80000000;
                    }

                    // Отправляем сообщение окну, которое находится в фокусе нажатие клавиши.
                    SendMessageW(gui_info.hwndFocus, wParam, hook_struct->vkCode, flags);

                    return TRUE;
                }
                break;
            }
        }
    }

    return CallNextHookEx(nullptr, code, wParam, lParam);
}

void ScreenWindowWin::Worker()
{
    //
    // Для минимизации задержек при отправке сообщений ввода устанавливаем
    // высокий приоритет для потока.
    //
    SetThreadPriority(Priority::Highest);

    const WCHAR class_name[] = L"Aspia.Video.Window.Class";

    HMODULE instance = nullptr;

    // Определяем дискриптор текущего модуля по адресу KeyboardHookProc.
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<const WCHAR*>(&KeyboardHookProc),
                            &instance))
    {
        LOG(WARNING) << "GetModuleHandleExW() failed: " << GetLastError();
    }

    WNDCLASSEXW window_class = { 0 };

    window_class.cbSize        = sizeof(window_class);
    window_class.style         = CS_BYTEALIGNWINDOW;
    window_class.lpfnWndProc   = WndProc;
    window_class.lpszClassName = class_name;
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    window_class.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hInstance     = instance;

    RegisterClassExW(&window_class);

    DWORD style = parent_ ? WS_CHILD : WS_OVERLAPPEDWINDOW;

    window_ = CreateWindowExW(0,
                              class_name,
                              L"Video Window",
                              style,
                              0, 0, CW_USEDEFAULT, CW_USEDEFAULT,
                              parent_,
                              nullptr,
                              instance,
                              this);
    DCHECK(window_) << "Unable to create screen window: " << GetLastError();

    ShowWindow(window_, SW_SHOW);
    UpdateWindow(window_);

    //
    // Для правильной обработки некоторых комбинаций нажатия клавиш необходимо
    // установить перехватчик, который будет предотвращать обработку этих
    // нажатий системой. К таким комбинациям относятся Left Win, Right Win,
    // Alt + Tab. Перехватчик определяет нажатия данных клавиш и отправляет
    // сообщение нажатия клавиши окну, которое находится в данный момент в фокусе.
    //
    HHOOK keyboard_hook = SetWindowsHookExW(WH_KEYBOARD_LL,
                                            KeyboardHookProc,
                                            instance,
                                            0);
    if (!keyboard_hook)
    {
        LOG(WARNING) << "SetWindowsHookExW() failed: " << GetLastError();
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (keyboard_hook)
        UnhookWindowsHookEx(keyboard_hook);

    UnregisterClassW(class_name, nullptr);

    std::async(std::launch::async, on_closed_);
}

void ScreenWindowWin::OnStart()
{
    // Nothing
}

void ScreenWindowWin::OnStop()
{
    DestroyWindow(window_);
}
