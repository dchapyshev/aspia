/*
* PROJECT:         Aspia Remote Desktop
* FILE:            client/screen_window.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "client/screen_window.h"

#include "base/scoped_select_object.h"

namespace aspia {

static const DWORD kKeyUpFlag = 0x80000000;
static const DWORD kKeyExtendedFlag = 0x1000000;

ScreenWindow::ScreenWindow(HWND parent, std::unique_ptr<Socket> sock) :
    parent_(parent),
    window_(nullptr),
    instance_(nullptr),
    keyboard_hook_(nullptr),
    image_buffer_(nullptr),
    background_brush_(CreateSolidBrush(RGB(25, 25, 25)))
{
    Client::OnEventCallback on_event_callback =
        std::bind(&ScreenWindow::OnClientEvent, this, std::placeholders::_1);

    client_.reset(new Client(std::move(sock), on_event_callback));
}

ScreenWindow::~ScreenWindow()
{
    if (!IsThreadTerminated())
    {
        Stop();
        WaitForEnd();
    }
}

void ScreenWindow::OnClientEvent(Client::Event type)
{
    if (type == Client::Event::NotConnected)
    {
        Stop();
    }
    else if (type == Client::Event::Disconnected)
    {
        Stop();
    }
    else if (type == Client::Event::BadAuth)
    {
        Stop();
    }
    else if (type == Client::Event::Connected)
    {
        Client::OnVideoUpdateCallback on_update_callback =
            std::bind(&ScreenWindow::OnVideoUpdate, this, std::placeholders::_1, std::placeholders::_2);

        Client::OnVideoResizeCallback on_resize_callback =
            std::bind(&ScreenWindow::OnVideoResize, this, std::placeholders::_1, std::placeholders::_2);

        client_->EnableVideoUpdate(proto::VIDEO_ENCODING_VP8,
                                   PixelFormat::MakeRGB555(),
                                   on_update_callback,
                                   on_resize_callback);

        Start();
    }
}

void ScreenWindow::AllocateBuffer(int align)
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
    } BitmapInfo;

    BitmapInfo bmi = { 0 };
    bmi.header.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.header.biBitCount    = pixel_format_.BitsPerPixel();
    bmi.header.biSizeImage   = pixel_format_.BytesPerPixel() * aligned_width * screen_size_.height();
    bmi.header.biPlanes      = 1;
    bmi.header.biWidth       = screen_size_.width();
    bmi.header.biHeight      = -screen_size_.height();
    bmi.header.biCompression = (pixel_format_.BitsPerPixel() == 8) ? BI_RGB : BI_BITFIELDS;

    if (pixel_format_.BitsPerPixel() == 8)
    {
        for (int i = 0; i < 256; ++i)
        {
            uint32_t red   = (i >> pixel_format_.RedShift())   & pixel_format_.RedMax();
            uint32_t green = (i >> pixel_format_.GreenShift()) & pixel_format_.GreenMax();
            uint32_t blue  = (i >> pixel_format_.BlueShift())  & pixel_format_.BlueMax();

            bmi.u.color[i].rgbRed   = red   * 0xFF / pixel_format_.RedMax();
            bmi.u.color[i].rgbGreen = green * 0xFF / pixel_format_.GreenMax();
            bmi.u.color[i].rgbBlue  = blue  * 0xFF / pixel_format_.BlueMax();
        }
    }
    else
    {
        bmi.u.mask.red   = pixel_format_.RedMax()   << pixel_format_.RedShift();
        bmi.u.mask.green = pixel_format_.GreenMax() << pixel_format_.GreenShift();
        bmi.u.mask.blue  = pixel_format_.BlueMax()  << pixel_format_.BlueShift();
    }

    target_bitmap_ = CreateDIBSection(memory_dc_,
                                      reinterpret_cast<PBITMAPINFO>(&bmi),
                                      DIB_RGB_COLORS,
                                      reinterpret_cast<void**>(&image_buffer_),
                                      nullptr,
                                      0);
}

void ScreenWindow::OnVideoUpdate(const uint8_t *buffer, const DesktopRegion &region)
{
    int bytes_per_pixel = pixel_format_.BytesPerPixel();
    int stride = screen_size_.width() * bytes_per_pixel;

    for (DesktopRegion::Iterator iter(region); !iter.IsAtEnd(); iter.Advance())
    {
        const DesktopRect &rect = iter.rect();

        const uint8_t *in = buffer + stride * rect.y() + rect.x() * bytes_per_pixel;
        uint8_t *out = image_buffer_ + stride * rect.y() + rect.x() * bytes_per_pixel;

        int height = rect.height();
        int width = rect.width() * bytes_per_pixel;

        for (int y = 0; y < height; ++y)
        {
            memcpy(out, in, width);

            in += stride;
            out += stride;
        }
    }

    InvalidateRect(window_, nullptr, FALSE);
}

void ScreenWindow::OnVideoResize(const DesktopSize &size, const PixelFormat &format)
{
    {
        LockGuard<Lock> guard(&image_buffer_lock_);

        screen_size_ = size;
        pixel_format_ = format;

        screen_dc_.set(CreateCompatibleDC(nullptr));
        memory_dc_.set(CreateCompatibleDC(screen_dc_));

        // Allocate 32 bit aligned image
        AllocateBuffer(32);
    }

    PostMessageW(window_, WM_SIZE, 0, 0);
}

void ScreenWindow::DrawBackground(HDC hdc, const RECT &paint_rect)
{
    RECT l_rect = { paint_rect.left,
                    paint_rect.top,
                    center_offset_.x(),
                    paint_rect.bottom };

    RECT r_rect = { center_offset_.x() + screen_size_.width(),
                    paint_rect.top,
                    paint_rect.right,
                    paint_rect.bottom };

    LONG l_rect_width = l_rect.right - l_rect.left;
    LONG r_rect_width = r_rect.right - r_rect.left;

    RECT t_rect = { paint_rect.left + l_rect_width,
                    paint_rect.top,
                    paint_rect.right - r_rect_width,
                    center_offset_.y() };

    RECT b_rect = { paint_rect.left + l_rect_width,
                    center_offset_.y() + screen_size_.height(),
                    paint_rect.right - r_rect_width,
                    paint_rect.bottom };

    FillRect(hdc, &l_rect, background_brush_);
    FillRect(hdc, &r_rect, background_brush_);
    FillRect(hdc, &t_rect, background_brush_);
    FillRect(hdc, &b_rect, background_brush_);
}

void ScreenWindow::OnPaint()
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(window_, &ps);

    if (screen_dc_)
    {
        DesktopPoint scroll_pos(0, 0);

        if (!center_offset_.x())
            scroll_pos.set_x(scroll_pos_.x());

        if (!center_offset_.y())
            scroll_pos.set_y(scroll_pos_.y());

        DrawBackground(hdc, ps.rcPaint);

        int width = ps.rcPaint.right - ps.rcPaint.left;
        int height = ps.rcPaint.bottom - ps.rcPaint.top;

        {
            LockGuard<Lock> guard(&image_buffer_lock_);

            {
                ScopedSelectObject select_object(memory_dc_, target_bitmap_);

                BitBlt(hdc,
                       ps.rcPaint.left + center_offset_.x(),
                       ps.rcPaint.top + center_offset_.y(),
                       width - (center_offset_.x() * 2),
                       height - (center_offset_.y() * 2),
                       memory_dc_,
                       ps.rcPaint.left + scroll_pos.x(),
                       ps.rcPaint.top + scroll_pos.y(),
                       SRCCOPY);
            }
        }
    }
    else
    {
        FillRect(hdc, &ps.rcPaint, background_brush_);
    }

    EndPaint(window_, &ps);
}

void ScreenWindow::OnCreate(HWND window, LPARAM lParam)
{
    ScreenWindow *self =
        reinterpret_cast<ScreenWindow*>(reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams);

    // Очищаем состояние ошибки.
    SetLastError(ERROR_SUCCESS);

    // Устанавливаем в качестве пользовательских данных указатель на экземпляр класса.
    LONG_PTR prev = SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));

    DWORD err = GetLastError();

    // Если предыдущее значение не равно нулю и возвращена ошибка.
    CHECK(!prev && err == ERROR_SUCCESS) << "SetWindowLongPtrW() failed: " << err;
}

void ScreenWindow::OnDestroy()
{
    if (keyboard_hook_)
        UnhookWindowsHookEx(keyboard_hook_);

    PostQuitMessage(0);
}

void ScreenWindow::UpdateScrollBars()
{
    DesktopSize scrollbar;

    if (client_size_.width() >= screen_size_.width())
        scrollbar.set_width(GetSystemMetrics(SM_CXHSCROLL));

    if (client_size_.height() >= screen_size_.height())
        scrollbar.set_height(GetSystemMetrics(SM_CYVSCROLL));

    SCROLLINFO si = { 0 };
    si.cbSize = sizeof(si);
    si.fMask  = SIF_PAGE | SIF_POS | SIF_RANGE;
    si.nMin   = 0;

    si.nMax   = screen_size_.width();
    si.nPage  = client_size_.width() + scrollbar.width();
    si.nPos   = scroll_pos_.x();
    SetScrollInfo(window_, SB_HORZ, &si, TRUE);

    si.nMax   = screen_size_.height();
    si.nPage  = client_size_.height() + scrollbar.height();
    si.nPos   = scroll_pos_.y();
    SetScrollInfo(window_, SB_VERT, &si, TRUE);
}

void ScreenWindow::Scroll(DesktopPoint &delta)
{
    if (delta.x())
    {
        delta.set_x(std::max(delta.x(), -scroll_pos_.x()));
        delta.set_x(std::min(delta.x(), screen_size_.width() - client_size_.width() - scroll_pos_.x()));
    }

    if (delta.y())
    {
        delta.set_y(std::max(delta.y(), -scroll_pos_.y()));
        delta.set_y(std::min(delta.y(), screen_size_.height() - client_size_.height() - scroll_pos_.y()));
    }

    if (delta.x() || delta.y())
    {
        scroll_pos_.set_x(scroll_pos_.x() + delta.x());
        scroll_pos_.set_y(scroll_pos_.y() + delta.y());

        ScrollWindowEx(window_,
                       -delta.x(),
                       -delta.y(),
                       nullptr,
                       nullptr,
                       nullptr,
                       nullptr,
                       SW_INVALIDATE);

        UpdateScrollBars();
    }
}

void ScreenWindow::OnHScroll(WPARAM wParam)
{
    DesktopPoint delta(0, 0);

    switch (LOWORD(wParam))
    {
        case SB_LINEUP:
            delta.set_x(-2);
            break;

        case SB_LINEDOWN:
            delta.set_x(2);
            break;

        case SB_PAGEUP:
            delta.set_x(client_size_.width() * -1 / 4);
            break;

        case SB_PAGEDOWN:
            delta.set_x(client_size_.width() * 1 / 4);
            break;

        case SB_THUMBPOSITION:
            // high word contains current position
            delta.set_x(HIWORD(wParam) - scroll_pos_.x());
            break;

        case SB_THUMBTRACK:
            // high word contains current position
            delta.set_x(HIWORD(wParam) - scroll_pos_.x());
            break;
    }

    Scroll(delta);
}

void ScreenWindow::OnVScroll(WPARAM wParam)
{
    DesktopPoint delta(0, 0);

    switch (LOWORD(wParam))
    {
        case SB_LINEUP:
            delta.set_y(-2);
            break;

        case SB_LINEDOWN:
            delta.set_y(2);
            break;

        case SB_PAGEUP:
            delta.set_y(client_size_.height() * -1 / 4);
            break;

        case SB_PAGEDOWN:
            delta.set_y(client_size_.height() * 1 / 4);
            break;

        case SB_THUMBPOSITION:
            // high word contains current position
            delta.set_y(HIWORD(wParam) - scroll_pos_.y());
            break;

        case SB_THUMBTRACK:
            // high word contains current position
            delta.set_y(HIWORD(wParam) - scroll_pos_.y());
            break;
    }

    Scroll(delta);
}

void ScreenWindow::OnSize()
{
    RECT rect = { 0 };

    GetClientRect(window_, &rect);

    client_size_.set_width(rect.right - rect.left);
    client_size_.set_height(rect.bottom - rect.top);

    if (screen_size_.width() < client_size_.width())
        center_offset_.set_x((client_size_.width() / 2) - (screen_size_.width() / 2));
    else
        center_offset_.set_x(0);

    if (screen_size_.height() < client_size_.height())
        center_offset_.set_y((client_size_.height() / 2) - (screen_size_.height() / 2));
    else
        center_offset_.set_y(0);

    DesktopPoint new_scroll_pos(std::max(0, std::min(scroll_pos_.x(), screen_size_.width() - std::max(client_size_.width(), 0))),
                                std::max(0, std::min(scroll_pos_.y(), screen_size_.height() - std::max(client_size_.height(), 0))));

    ScrollWindowEx(window_,
                   scroll_pos_.x() - new_scroll_pos.x(),
                   scroll_pos_.y() - new_scroll_pos.y(),
                   nullptr,
                   nullptr,
                   nullptr,
                   nullptr,
                   SW_INVALIDATE);

    scroll_pos_ = new_scroll_pos;

    UpdateScrollBars();

    InvalidateRect(window_, nullptr, FALSE);
}

void ScreenWindow::OnClose()
{
    client_.reset();
}

void ScreenWindow::OnMouseMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (!client_)
        return;

    POINT pos = { LOWORD(lParam), HIWORD(lParam) };

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

        if (!ScreenToClient(window_, &pos))
            return;

        wheel_mask = proto::PointerEvent::WHEEL_DOWN | proto::PointerEvent::WHEEL_UP;
    }

    pos.x -= center_offset_.x() - std::abs(scroll_pos_.x());
    pos.y -= center_offset_.y() - std::abs(scroll_pos_.y());

    if (pos.x > 0 && pos.x <= screen_size_.width() &&
        pos.y > 0 && pos.y <= screen_size_.height())
    {
        client_->SendPointerEvent(pos.x, pos.y, mask);

        if (mask & wheel_mask)
        {
            client_->SendPointerEvent(pos.x, pos.y, mask & ~wheel_mask);
        }
    }
}

void ScreenWindow::SendCAD()
{
    if (client_)
    {
        client_->SendKeyEvent(VK_CONTROL, false, true);
        client_->SendKeyEvent(VK_MENU, false, true);
        client_->SendKeyEvent(VK_DELETE, true, true);

        client_->SendKeyEvent(VK_CONTROL, false, false);
        client_->SendKeyEvent(VK_MENU, false, false);
        client_->SendKeyEvent(VK_DELETE, true, false);
    }
}

void ScreenWindow::OnKeyMessage(WPARAM wParam, LPARAM lParam)
{
    if (client_)
    {
        uint32_t key_data = static_cast<uint32_t>(lParam);

        bool pressed = ((key_data & kKeyUpFlag) == 0);
        bool extended = ((key_data & kKeyExtendedFlag) != 0);

        uint8_t key = static_cast<uint8_t>(static_cast<uint32_t>(wParam) & 255);

        client_->SendKeyEvent(key, extended, pressed);
    }
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
                        flags |= kKeyExtendedFlag;
                    }

                    if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
                    {
                        // Бит 31 установленный в 1 означает, что клавиша отжата.
                        flags |= kKeyUpFlag;
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

void ScreenWindow::OnActivate(WPARAM wParam)
{
    // При активации окна устанавливаем хуки, а при деактивации убираем их.
    if (LOWORD(wParam) == WA_ACTIVE || LOWORD(wParam) == WA_CLICKACTIVE)
    {
        //
        // Для правильной обработки некоторых комбинаций нажатия клавиш необходимо
        // установить перехватчик, который будет предотвращать обработку этих
        // нажатий системой. К таким комбинациям относятся Left Win, Right Win,
        // Alt + Tab. Перехватчик определяет нажатия данных клавиш и отправляет
        // сообщение нажатия клавиши окну, которое находится в данный момент в фокусе.
        //
        keyboard_hook_ = SetWindowsHookExW(WH_KEYBOARD_LL,
                                           KeyboardHookProc,
                                           instance_,
                                           0);
        if (!keyboard_hook_)
        {
            LOG(WARNING) << "SetWindowsHookExW() failed: " << GetLastError();
        }
    }
    else if (LOWORD(wParam) == WA_INACTIVE)
    {
        if (keyboard_hook_)
            UnhookWindowsHookEx(keyboard_hook_);
    }
}

LRESULT ScreenWindow::ProcessWindowMessage(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_PAINT:
            OnPaint();
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_MOUSEWHEEL:
        case WM_LBUTTONUP:
        case WM_LBUTTONDOWN:
        case WM_MOUSEMOVE:
        case WM_MBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_RBUTTONDOWN:
            OnMouseMessage(msg, wParam, lParam);
            return 1;

        case WM_KEYUP:
        case WM_KEYDOWN:
        case WM_SYSKEYUP:
        case WM_SYSKEYDOWN:
            OnKeyMessage(wParam, lParam);
            return 1;

        case WM_CHAR:
        case WM_SYSCHAR:
        case WM_DEADCHAR:
        case WM_SYSDEADCHAR:
            return 1;

        case WM_SIZE:
        case WM_WINDOWPOSCHANGED:
            OnSize();
            break;

        case WM_HSCROLL:
            OnHScroll(wParam);
            return 1;

        case WM_VSCROLL:
            OnVScroll(wParam);
            return 1;

        case WM_ACTIVATE:
            OnActivate(wParam);
            break;

        case WM_CREATE:
            OnCreate(window, lParam);
            break;

        case WM_DESTROY:
            OnDestroy();
            break;

        case WM_CLOSE:
            OnClose();
            break;
    }

    return DefWindowProcW(window, msg, wParam, lParam);
}

// static
LRESULT CALLBACK ScreenWindow::WindowProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    ScreenWindow *self;

    if (msg == WM_CREATE)
    {
        self = reinterpret_cast<ScreenWindow*>(reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams);
    }
    else
    {
        self = reinterpret_cast<ScreenWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    }

    return self->ProcessWindowMessage(window, msg, wParam, lParam);
}

void ScreenWindow::Worker()
{
    const WCHAR kWindowClassName[] = L"Aspia.Video.Window.Class";

    // Определяем дискриптор текущего модуля по адресу KeyboardHookProc.
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<const WCHAR*>(&KeyboardHookProc),
                            &instance_))
    {
        LOG(WARNING) << "GetModuleHandleExW() failed: " << GetLastError();
    }

    WNDCLASSEXW window_class = { 0 };

    window_class.cbSize        = sizeof(window_class);
    window_class.style         = CS_VREDRAW | CS_HREDRAW;
    window_class.lpfnWndProc   = WindowProc;
    window_class.lpszClassName = kWindowClassName;
    window_class.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hInstance     = instance_;

    RegisterClassExW(&window_class);

    window_ = CreateWindowExW(0,
                              kWindowClassName,
                              L"Aspia Remote Desktop",
                              (parent_ ? WS_CHILD : WS_OVERLAPPEDWINDOW),
                              0, 0, 400, 200,
                              parent_,
                              nullptr,
                              instance_,
                              this);
    CHECK(window_) << "Unable to create screen window: " << GetLastError();

    ShowWindow(window_, SW_SHOW);
    UpdateWindow(window_);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DestroyWindow(window_);

    UnregisterClassW(kWindowClassName, nullptr);
}

void ScreenWindow::OnStop()
{
    PostMessageW(window_, WM_CLOSE, 0, 0);
}

} // namespace aspia
