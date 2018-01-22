//
// PROJECT:         Aspia
// FILE:            ui/desktop/viewer_window.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/desktop/viewer_window.h"
#include "ui/about_dialog.h"
#include "ui/resource.h"
#include "base/strings/unicode.h"
#include "desktop_capture/cursor.h"

namespace aspia {

static const DWORD kKeyUpFlag = 0x80000000;
static const DWORD kKeyExtendedFlag = 0x1000000;

static const DesktopSize kVideoWindowSize(400, 280);

ViewerWindow::ViewerWindow(proto::ClientConfig* config, Delegate* delegate)
    : config_(config),
      delegate_(delegate),
      video_window_(this)
{
    DCHECK(config_);
    DCHECK(delegate_);

    ui_thread_.Start(MessageLoop::TYPE_UI, this);
}

ViewerWindow::~ViewerWindow()
{
    ui_thread_.Stop();
}

void ViewerWindow::OnBeforeThreadRunning()
{
    runner_ = ui_thread_.message_loop_proxy();
    DCHECK(runner_);

    if (!Create(nullptr, rcDefault, nullptr, WS_OVERLAPPEDWINDOW))
    {
        PLOG(LS_ERROR) << "Viewer window not created";
        runner_->PostQuit();
    }
    else
    {
        ShowWindow(SW_SHOW);
        UpdateWindow();
    }
}

void ViewerWindow::OnAfterThreadRunning()
{
    DestroyWindow();
}

DesktopFrame* ViewerWindow::Frame()
{
    return video_window_.Frame();
}

void ViewerWindow::DrawFrame()
{
    video_window_.DrawFrame();
}

LRESULT ViewerWindow::OnVideoFrameResize(
    UINT /* message */, WPARAM wparam, LPARAM lparam, BOOL& /* handled */)
{
    const DesktopSize* size = reinterpret_cast<const DesktopSize*>(wparam);
    const PixelFormat* format = reinterpret_cast<const PixelFormat*>(lparam);

    int show_scroll_bars = -1;

    if (!size->IsEqual(video_window_.FrameSize()))
    {
        video_window_.ShowScrollBar(SB_BOTH, FALSE);
        show_scroll_bars = DoAutoSize(*size);
    }

    video_window_.ResizeFrame(*size, *format);

    if (show_scroll_bars != -1)
        video_window_.ShowScrollBar(show_scroll_bars, FALSE);

    return 0;
}

void ViewerWindow::ResizeFrame(const DesktopSize& size, const PixelFormat& format)
{
    // ResizeFrame method is called from another thread.
    // We need to move the action to UI thread.
    SendMessageW(kResizeFrameMessage,
                 reinterpret_cast<WPARAM>(&size),
                 reinterpret_cast<LPARAM>(&format));
}

void ViewerWindow::InjectMouseCursor(std::shared_ptr<MouseCursor> mouse_cursor)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&ViewerWindow::InjectMouseCursor, this, mouse_cursor));
        return;
    }

    ScopedGetDC desktop_dc(nullptr);

    ScopedHCURSOR cursor(CreateHCursorFromMouseCursor(desktop_dc, *mouse_cursor));

    if (!cursor.IsValid())
    {
        // If we can not get a remote cursor, then we use the default cursor.
        cursor = LoadCursorW(nullptr, IDC_ARROW);
    }

    SetClassLongPtrW(video_window_,
                     GCLP_HCURSOR,
                     reinterpret_cast<LONG_PTR>(cursor.Get()));
}

void ViewerWindow::InjectClipboardEvent(
    std::shared_ptr<proto::desktop::ClipboardEvent> clipboard_event)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&ViewerWindow::InjectClipboardEvent, this, clipboard_event));
        return;
    }

    clipboard_.InjectClipboardEvent(clipboard_event);
}

LRESULT ViewerWindow::OnCreate(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    small_icon_ = AtlLoadIconImage(IDI_MAIN,
                                   LR_CREATEDIBSECTION,
                                   GetSystemMetrics(SM_CXSMICON),
                                   GetSystemMetrics(SM_CYSMICON));
    SetIcon(small_icon_, FALSE);

    big_icon_ = AtlLoadIconImage(IDI_MAIN,
                                 LR_CREATEDIBSECTION,
                                 GetSystemMetrics(SM_CXICON),
                                 GetSystemMetrics(SM_CYICON));
    SetIcon(big_icon_, TRUE);

    SetCursor(LoadCursorW(nullptr, IDC_ARROW));

    CString app_name;
    app_name.LoadStringW(IDS_APPLICATION_NAME);

    CString title(config_->address().c_str());
    title += L" - ";
    title += app_name;

    SetWindowTextW(title);

    toolbar_.CreateViewerToolBar(*this, config_->session_type());
    video_window_.Create(*this, rcDefault, nullptr, WS_CHILD | WS_VISIBLE);

    DoAutoSize(kVideoWindowSize);

    ApplyConfig(config_->desktop_session());

    return 0;
}

LRESULT ViewerWindow::OnClose(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    delegate_->OnWindowClose();
    return 0;
}

LRESULT ViewerWindow::OnDestroy(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    toolbar_.DestroyWindow();
    video_window_.DestroyWindow();
    return 0;
}

LRESULT ViewerWindow::OnSize(
    UINT /* message */, WPARAM wparam, LPARAM /* lparam */, BOOL& /* handled */)
{
    if (wparam == SIZE_MINIMIZED)
        return 0;

    toolbar_.AutoSize();

    CRect toolbar_rect;
    toolbar_.GetWindowRect(toolbar_rect);

    CRect client_rect;
    GetClientRect(client_rect);

    video_window_.MoveWindow(0,
                             toolbar_rect.Height(),
                             client_rect.Width(),
                             client_rect.Height() - toolbar_rect.Height(),
                             TRUE);
    return 0;
}

LRESULT ViewerWindow::OnGetMinMaxInfo(
    UINT /* message */, WPARAM /* wparam */, LPARAM lparam, BOOL& /* handled */)
{
    LPMINMAXINFO mmi = reinterpret_cast<LPMINMAXINFO>(lparam);

    mmi->ptMinTrackSize.x = kVideoWindowSize.Width();
    mmi->ptMinTrackSize.y = kVideoWindowSize.Height();

    return 0;
}

static LRESULT CALLBACK KeyboardHookProc(INT code, WPARAM wparam, LPARAM lparam)
{
    if (code == HC_ACTION)
    {
        GUITHREADINFO gui_info = { 0 };

        gui_info.cbSize = sizeof(gui_info);

        if (GetGUIThreadInfo(0, &gui_info) && gui_info.hwndFocus)
        {
            PKBDLLHOOKSTRUCT hook_struct = reinterpret_cast<PKBDLLHOOKSTRUCT>(lparam);

            switch (hook_struct->vkCode)
            {
                case VK_RETURN:
                case VK_TAB:
                case VK_LWIN:
                case VK_RWIN:
                {
                    // If the TAB key is pressed and ALT is not pressed.
                    if (hook_struct->vkCode == VK_TAB && !(hook_struct->flags & LLKHF_ALTDOWN))
                        break;

                    DWORD flags = 0;

                    if (hook_struct->flags & LLKHF_EXTENDED)
                    {
                        // Bit 24 set to 1 means that it's an extended key code.
                        flags |= kKeyExtendedFlag;
                    }

                    if (wparam == WM_KEYUP || wparam == WM_SYSKEYUP)
                    {
                        // Bit 31 set to 1 means that the key is not pressed.
                        flags |= kKeyUpFlag;
                    }

                    // Send the message to the window that is in focus.
                    SendMessageW(gui_info.hwndFocus,
                                 static_cast<UINT>(wparam),
                                 hook_struct->vkCode,
                                 flags);

                    return TRUE;
                }

                default:
                    break;
            }
        }
    }

    return CallNextHookEx(nullptr, code, wparam, lparam);
}

LRESULT ViewerWindow::OnActivate(
    UINT /* message */, WPARAM wparam, LPARAM /* lparam */, BOOL& /* handled */)
{
    const UINT state = LOWORD(wparam);

    if (state == WA_ACTIVE || state == WA_CLICKACTIVE)
    {
        // To properly handle some key combinations, we need to set a hook
        // that will prevent the system processing these presses.
        // These combinations include Left Win, Right Win, Alt + Tab.
        keyboard_hook_.Reset(SetWindowsHookExW(WH_KEYBOARD_LL,
                                               KeyboardHookProc,
                                               GetModuleHandleW(nullptr),
                                               0));
        video_window_.SetHasFocus(true);
    }
    else if (state == WA_INACTIVE)
    {
        keyboard_hook_.Reset();
        video_window_.SetHasFocus(false);
    }

    return 0;
}

LRESULT ViewerWindow::OnKeyboard(
    UINT /* message */, WPARAM wparam, LPARAM lparam, BOOL& /* handled */)
{
    const uint8_t key_code = static_cast<uint8_t>(static_cast<uint32_t>(wparam) & 255);

    // We do not pass CapsLock and NoLock directly. Instead, when sending each
    // keystroke event, a flag is set that indicates the status of these keys.
    if (key_code != VK_CAPITAL && key_code != VK_NUMLOCK)
    {
        const uint32_t key_data = static_cast<uint32_t>(lparam);

        uint32_t flags = 0;

        flags |= ((key_data & kKeyUpFlag) == 0) ? proto::desktop::KeyEvent::PRESSED : 0;
        flags |= ((key_data & kKeyExtendedFlag) != 0) ? proto::desktop::KeyEvent::EXTENDED : 0;
        flags |= (GetKeyState(VK_CAPITAL) != 0) ? proto::desktop::KeyEvent::CAPSLOCK : 0;
        flags |= (GetKeyState(VK_NUMLOCK) != 0) ? proto::desktop::KeyEvent::NUMLOCK : 0;

        delegate_->OnKeyEvent(key_code, flags);
    }

    return 0;
}

LRESULT ViewerWindow::OnSkipMessage(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    // Nothing
    return 0;
}

LRESULT ViewerWindow::OnSetFocus(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    video_window_.SetHasFocus(true);
    return 0;
}

LRESULT ViewerWindow::OnKillFocus(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    video_window_.SetHasFocus(false);
    return 0;
}

void ViewerWindow::ApplyConfig(const proto::desktop::Config& config)
{
    if (!(config.flags() & proto::desktop::Config::ENABLE_CURSOR_SHAPE))
    {
        SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    }

    if (config.flags() & proto::desktop::Config::ENABLE_CLIPBOARD)
    {
        clipboard_.Start(std::bind(&ViewerWindow::Delegate::OnClipboardEvent,
                                   delegate_,
                                   std::placeholders::_1));
    }
    else
    {
        clipboard_.Stop();
    }
}

LRESULT ViewerWindow::OnSettingsButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    SettingsDialog dialog(config_->session_type(), config_->desktop_session());

    if (dialog.DoModal(*this) == IDOK)
    {
        config_->mutable_desktop_session()->CopyFrom(dialog.Config());

        ApplyConfig(config_->desktop_session());
        delegate_->OnConfigChange(config_->desktop_session());
    }

    return 0;
}

LRESULT ViewerWindow::OnAboutButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    AboutDialog().DoModal(*this);
    return 0;
}

LRESULT ViewerWindow::OnExitButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    PostMessageW(WM_CLOSE);
    return 0;
}

CSize ViewerWindow::GetMonitorSize() const
{
    CRect screen_rect;

    HMONITOR monitor = MonitorFromWindow(*this, MONITOR_DEFAULTTONEAREST);

    MONITORINFO info;
    info.cbSize = sizeof(info);

    if (GetMonitorInfoW(monitor, &info))
    {
        screen_rect = info.rcWork;
    }
    else
    {
        if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, screen_rect, 0))
        {
            if (!::GetClientRect(GetDesktopWindow(), screen_rect))
                return CSize();
        }
    }

    return screen_rect.Size();
}

CSize ViewerWindow::CalculateWindowSize(const DesktopSize &video_frame_size) const
{
    CRect full_rect;
    GetWindowRect(full_rect);

    CRect client_rect;
    GetClientRect(client_rect);

    CRect toolbar_rect;
    toolbar_.GetWindowRect(toolbar_rect);

    return CSize(video_frame_size.Width() + full_rect.Width() - client_rect.Width(),
                 video_frame_size.Height() + full_rect.Height() - client_rect.Height() +
                 toolbar_rect.Height());
}

int ViewerWindow::DoAutoSize(const DesktopSize &video_frame_size)
{
    if (toolbar_.IsButtonChecked(ID_FULLSCREEN))
        DoFullScreen(false);

    const CSize screen_size = GetMonitorSize();
    const CSize window_size = CalculateWindowSize(video_frame_size);

    WINDOWPLACEMENT wp = { 0 };
    wp.length = sizeof(wp);

    if (!GetWindowPlacement(&wp))
        return -1;

    if (window_size.cx < screen_size.cx && window_size.cy < screen_size.cy)
    {
        if (wp.showCmd != SW_MAXIMIZE)
        {
            SetWindowPos(nullptr,
                         (screen_size.cx - window_size.cx) / 2,
                         (screen_size.cy - window_size.cy) / 2,
                         window_size.cx,
                         window_size.cy,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }

        return SB_BOTH;
    }

    if (wp.showCmd != SW_MAXIMIZE)
        ShowWindow(SW_MAXIMIZE);

    if (window_size.cx < screen_size.cx)
        return SB_HORZ;

    if (window_size.cy < screen_size.cy)
        return SB_VERT;

    return -1;
}

LRESULT ViewerWindow::OnAutoSizeButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    DesktopSize video_frame_size = video_window_.FrameSize();

    if (!video_frame_size.IsEmpty())
    {
        if (DoAutoSize(video_frame_size) != -1)
            video_window_.ShowScrollBar(SB_BOTH, FALSE);
    }

    return 0;
}

void ViewerWindow::DoFullScreen(bool fullscreen)
{
    toolbar_.CheckButton(ID_FULLSCREEN, fullscreen);

    if (fullscreen)
    {
        if (GetWindowPlacement(&window_pos_))
        {
            CRect screen_rect;

            HMONITOR monitor = MonitorFromWindow(*this, MONITOR_DEFAULTTONEAREST);

            MONITORINFO info;
            info.cbSize = sizeof(info);

            if (GetMonitorInfoW(monitor, &info))
            {
                screen_rect = info.rcMonitor;
            }
            else
            {
                if (!::GetWindowRect(GetDesktopWindow(), screen_rect))
                    return;
            }

            ModifyStyle(WS_CAPTION | WS_BORDER | WS_THICKFRAME | WS_MAXIMIZEBOX, WS_MAXIMIZE);
            ModifyStyleEx(0, WS_EX_TOPMOST);

            SetWindowPos(nullptr,
                         screen_rect.left,
                         screen_rect.top,
                         screen_rect.Width(),
                         screen_rect.Height(),
                         SWP_SHOWWINDOW);
        }
    }
    else
    {
        ModifyStyle(WS_MAXIMIZE, WS_CAPTION | WS_BORDER | WS_THICKFRAME | WS_MAXIMIZEBOX);
        ModifyStyleEx(WS_EX_TOPMOST, 0);

        SetWindowPlacement(&window_pos_);
    }
}

LRESULT ViewerWindow::OnFullScreenButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    DoFullScreen(toolbar_.IsButtonChecked(ID_FULLSCREEN));
    return 0;
}

LRESULT ViewerWindow::OnDropDownButton(
    WORD /* notify_code */, WORD control_id, HWND /* control */, BOOL& /* handled */)
{
    CRect rect;
    toolbar_.GetRect(control_id, &rect);
    ShowDropDownMenu(control_id, &rect);
    return 0;
}

LRESULT ViewerWindow::OnCADButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    delegate_->OnKeyEvent(VK_CONTROL, proto::desktop::KeyEvent::PRESSED);
    delegate_->OnKeyEvent(VK_MENU, proto::desktop::KeyEvent::PRESSED);
    delegate_->OnKeyEvent(VK_DELETE, proto::desktop::KeyEvent::EXTENDED |
                                     proto::desktop::KeyEvent::PRESSED);

    delegate_->OnKeyEvent(VK_CONTROL, 0);
    delegate_->OnKeyEvent(VK_MENU, 0);
    delegate_->OnKeyEvent(VK_DELETE, proto::desktop::KeyEvent::EXTENDED);

    return 0;
}

LRESULT ViewerWindow::OnKeyButton(
    WORD /* notify_code */, WORD control_id, HWND /* control */, BOOL& /* handled */)
{
    switch (control_id)
    {
        case ID_KEY_CTRL_ESC:
        {
            delegate_->OnKeyEvent(VK_CONTROL, proto::desktop::KeyEvent::PRESSED);
            delegate_->OnKeyEvent(VK_ESCAPE, proto::desktop::KeyEvent::PRESSED);

            delegate_->OnKeyEvent(VK_CONTROL, 0);
            delegate_->OnKeyEvent(VK_ESCAPE, 0);
        }
        break;

        case ID_KEY_ALT_TAB:
        {
            delegate_->OnKeyEvent(VK_MENU, proto::desktop::KeyEvent::PRESSED);
            delegate_->OnKeyEvent(VK_TAB, proto::desktop::KeyEvent::PRESSED);

            delegate_->OnKeyEvent(VK_TAB, 0);
            delegate_->OnKeyEvent(VK_MENU, 0);
        }
        break;

        case ID_KEY_ALT_SHIFT_TAB:
        {
            delegate_->OnKeyEvent(VK_MENU, proto::desktop::KeyEvent::PRESSED);
            delegate_->OnKeyEvent(VK_SHIFT, proto::desktop::KeyEvent::PRESSED);
            delegate_->OnKeyEvent(VK_TAB, proto::desktop::KeyEvent::PRESSED);

            delegate_->OnKeyEvent(VK_TAB, 0);
            delegate_->OnKeyEvent(VK_SHIFT, 0);
            delegate_->OnKeyEvent(VK_MENU, 0);
        }
        break;

        case ID_KEY_PRINTSCREEN:
        {
            delegate_->OnKeyEvent(VK_SNAPSHOT, proto::desktop::KeyEvent::PRESSED);
            delegate_->OnKeyEvent(VK_SNAPSHOT, 0);
        }
        break;

        case ID_KEY_ALT_PRINTSCREEN:
        {
            delegate_->OnKeyEvent(VK_MENU, proto::desktop::KeyEvent::PRESSED);
            delegate_->OnKeyEvent(VK_SNAPSHOT, proto::desktop::KeyEvent::PRESSED);

            delegate_->OnKeyEvent(VK_SNAPSHOT, 0);
            delegate_->OnKeyEvent(VK_MENU, 0);
        }
        break;

        case ID_KEY_CTRL_ALT_F12:
        {
            delegate_->OnKeyEvent(VK_CONTROL, proto::desktop::KeyEvent::PRESSED);
            delegate_->OnKeyEvent(VK_MENU, proto::desktop::KeyEvent::PRESSED);
            delegate_->OnKeyEvent(VK_F12, proto::desktop::KeyEvent::PRESSED);

            delegate_->OnKeyEvent(VK_F12, 0);
            delegate_->OnKeyEvent(VK_MENU, 0);
            delegate_->OnKeyEvent(VK_CONTROL, 0);
        }
        break;

        case ID_KEY_F12:
        {
            delegate_->OnKeyEvent(VK_F12, proto::desktop::KeyEvent::PRESSED);
            delegate_->OnKeyEvent(VK_F12, 0);
        }
        break;

        case ID_KEY_CTRL_F12:
        {
            delegate_->OnKeyEvent(VK_CONTROL, proto::desktop::KeyEvent::PRESSED);
            delegate_->OnKeyEvent(VK_F12, proto::desktop::KeyEvent::PRESSED);

            delegate_->OnKeyEvent(VK_F12, 0);
            delegate_->OnKeyEvent(VK_CONTROL, 0);
        }
        break;

        default:
            break;
    }

    return 0;
}

LRESULT ViewerWindow::OnToolBarDropDown(int /* control_id */, LPNMHDR hdr, BOOL& /* handled */)
{
    LPNMTOOLBARW header = reinterpret_cast<LPNMTOOLBARW>(hdr);
    ShowDropDownMenu(header->iItem, &header->rcButton);
    return 0;
}

void ViewerWindow::ShowDropDownMenu(int button_id, RECT* button_rect)
{
    if (button_id != ID_SHORTCUTS)
        return;

    if (toolbar_.MapWindowPoints(HWND_DESKTOP, button_rect))
    {
        TPMPARAMS tpm;
        tpm.cbSize = sizeof(TPMPARAMS);
        tpm.rcExclude = *button_rect;

        CMenu menu;
        menu.LoadMenuW(IDR_SHORTCUTS);

        CMenuHandle pupup = menu.GetSubMenu(0);

        pupup.TrackPopupMenuEx(TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL,
                               button_rect->left,
                               button_rect->bottom,
                               *this,
                               &tpm);
    }
}

void ViewerWindow::OnPointerEvent(const DesktopPoint& pos, uint32_t mask)
{
    delegate_->OnPointerEvent(pos, mask);
}

} // namespace aspia
