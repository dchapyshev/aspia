//
// PROJECT:         Aspia Remote Desktop
// FILE:            gui/viewer_window.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "gui/viewer_window.h"

#include "gui/resource.h"
#include "base/clipboard.h"
#include "base/unicode.h"
#include "desktop_capture/cursor.h"
#include "proto/proto.pb.h"

namespace aspia {

extern CIcon _small_icon;
extern CIcon _big_icon;

static const DWORD kKeyUpFlag = 0x80000000;
static const DWORD kKeyExtendedFlag = 0x1000000;

static const CSize kVideoWindowSize(400, 280);

ViewerWindow::ViewerWindow(CWindow *parent, Client *client, ClientConfig *config) :
    parent_(parent),
    client_(client),
    config_(config),
    video_window_(client)
{
    memset(&window_pos_, 0, sizeof(window_pos_));
    Start();
}

ViewerWindow::~ViewerWindow()
{
    if (IsActiveThread())
    {
        Stop();
        WaitForEnd();
    }
}

void ViewerWindow::OnVideoUpdate(const uint8_t *buffer, const DesktopRegion &region)
{
    video_window_.OnVideoUpdate(buffer, region);
}

void ViewerWindow::OnVideoResize(const DesktopSize &size, const PixelFormat &format)
{
    CSize current = video_window_.GetSize();

    DoAutoSize(CSize(size.Width(), size.Height()),
               (current.cx == 0 && current.cy == 0) ? true : false);

    video_window_.OnVideoResize(size, format);
    video_window_.ShowScrollBar(SB_BOTH, FALSE);
}

void ViewerWindow::OnCursorUpdate(const MouseCursor *mouse_cursor)
{
    ScopedGetDC desktop_dc(nullptr);

    cursor_ = CreateHCursorFromMouseCursor(desktop_dc, *mouse_cursor);
    if (cursor_)
    {
        SetClassLongPtrW(video_window_,
                         GCLP_HCURSOR,
                         reinterpret_cast<LONG_PTR>(cursor_.m_hCursor));
    }
}

int ViewerWindow::OnCreate(LPCREATESTRUCT create_struct)
{
    SetIcon(_small_icon, FALSE);
    SetIcon(_big_icon, TRUE);

    CString app_name;
    app_name.LoadStringW(IDS_APPLICATION_NAME);

    std::wstring title(UNICODEfromUTF8(config_->RemoteAddress()));

    title += L" - ";
    title += app_name;

    SetWindowTextW(title.c_str());

    toolbar_.Create(*this);
    video_window_.Create(*this);

    DoAutoSize(kVideoWindowSize, true);

    return 0;
}

void ViewerWindow::OnDestroy()
{
    PostQuitMessage(0);
}

void ViewerWindow::OnClose()
{
    parent_->PostMessageW(WM_APP);

    video_window_.DestroyWindow();
    DestroyWindow();
}

void ViewerWindow::OnSize(UINT type, CSize size)
{
    toolbar_.AutoSize();

    CRect rc;
    if (toolbar_.GetWindowRect(&rc))
    {
        int height = rc.Height();

        if (GetClientRect(&rc))
        {
            video_window_.MoveWindow(0, height, rc.Width(), rc.Height() - height);
        }
    }
}

void ViewerWindow::OnGetMinMaxInfo(LPMINMAXINFO mmi)
{
    // Задаем минимальные размеры окна.
    mmi->ptMinTrackSize.x = kVideoWindowSize.cx; // Ширина.
    mmi->ptMinTrackSize.y = kVideoWindowSize.cy; // Высота.
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

void ViewerWindow::OnActivate(UINT state, BOOL minimized, CWindow other_window)
{
    // При активации окна устанавливаем хуки, а при деактивации убираем их.
    if (state == WA_ACTIVE || state == WA_CLICKACTIVE)
    {
        //
        // Для правильной обработки некоторых комбинаций нажатия клавиш необходимо
        // установить перехватчик, который будет предотвращать обработку этих
        // нажатий системой. К таким комбинациям относятся Left Win, Right Win,
        // Alt + Tab. Перехватчик определяет нажатия данных клавиш и отправляет
        // сообщение нажатия клавиши окну, которое находится в данный момент в фокусе.
        //
        keyboard_hook_.Set(WH_KEYBOARD_LL, KeyboardHookProc);

        video_window_.HasFocus(true);
    }
    else if (state == WA_INACTIVE)
    {
        keyboard_hook_.Reset();

        video_window_.HasFocus(false);
    }
}

LRESULT ViewerWindow::OnKeyboard(UINT msg, WPARAM wParam, LPARAM lParam, BOOL &handled)
{
    uint32_t key_data = static_cast<uint32_t>(lParam);

    uint32_t flags = 0;

    flags |= ((key_data & kKeyUpFlag) == 0) ? proto::KeyEvent::PRESSED : 0;
    flags |= ((key_data & kKeyExtendedFlag) != 0) ? proto::KeyEvent::EXTENDED : 0;

    uint8_t key = static_cast<uint8_t>(static_cast<uint32_t>(wParam) & 255);

    client_->SendKeyEvent(key, flags);

    return 0;
}

LRESULT ViewerWindow::OnSkipMessage(UINT msg, WPARAM wParam, LPARAM lParam, BOOL &handled)
{
    return 0;
}

void ViewerWindow::OnSetFocus(CWindow old_window)
{
    video_window_.HasFocus(true);
}

void ViewerWindow::OnKillFocus(CWindow focus_window)
{
    video_window_.HasFocus(false);
}

LRESULT ViewerWindow::OnSettingsButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled)
{
    SettingsDialog dialog;
    dialog.DoModal(*this, reinterpret_cast<LPARAM>(config_->mutable_screen_config()));

    if (!config_->screen_config().IsEqualTo(dialog.GetConfig()))
    {
        const ScreenConfig &config = dialog.GetConfig();

        config_->mutable_screen_config()->Set(config);
        client_->ApplyScreenConfig(config);

        if (!config.ShowRemoteCursor())
        {
            cursor_ = LoadCursorW(nullptr, IDC_ARROW);

            SetClassLongPtrW(video_window_,
                             GCLP_HCURSOR,
                             reinterpret_cast<LONG_PTR>(cursor_.m_hCursor));
        }
    }

    return 0;
}

LRESULT ViewerWindow::OnBellButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled)
{
    client_->SendBellEvent();
    return 0;
}

LRESULT ViewerWindow::OnSendClipboardButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled)
{
    client_->SendClipboard(Clipboard::Get());
    return 0;
}

LRESULT ViewerWindow::OnRecvClipboardButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled)
{
    client_->SendClipboardControl(proto::ClipboardControl::REQUESTED);
    return 0;
}

LRESULT ViewerWindow::OnAboutButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled)
{
    AboutDialog dialog;
    dialog.DoModal();
    return 0;
}

LRESULT ViewerWindow::OnExitButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled)
{
    PostMessageW(WM_CLOSE);
    return 0;
}

void ViewerWindow::DoAutoSize(const CSize &video_size, bool move)
{
    if (toolbar_.IsButtonChecked(ID_FULLSCREEN))
    {
        DoFullScreen(FALSE);
    }

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
        if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &screen_rect, 0))
        {
            if (!CWindow(GetDesktopWindow()).GetClientRect(&screen_rect))
                return;
        }
    }

    CSize border_size;
    border_size.cx = (GetSystemMetrics(SM_CXSIZEFRAME) * 2);
    border_size.cy = (GetSystemMetrics(SM_CYSIZEFRAME) * 2) + GetSystemMetrics(SM_CYSIZE);

    CRect toolbar_rect;
    toolbar_.GetWindowRect(&toolbar_rect);

    int width  = video_size.cx + border_size.cx + 2;
    int height = video_size.cy + border_size.cy + toolbar_rect.Height() + 2;

    if (width < screen_rect.Width() && height < screen_rect.Height())
    {
        CRect window_rect;
        window_rect.left   = (screen_rect.Width() - width) / 2;
        window_rect.top    = (screen_rect.Height() - height) / 2;
        window_rect.right  = window_rect.left + width;
        window_rect.bottom = window_rect.top + height;

        SetWindowPos(nullptr, window_rect, SWP_NOZORDER | SWP_NOACTIVATE | (!move ? SWP_NOMOVE : 0));
        video_window_.ShowScrollBar(SB_BOTH, FALSE);
    }
    else
    {
        ShowWindow(SW_MAXIMIZE);
    }
}

LRESULT ViewerWindow::OnAutoSizeButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled)
{
    CSize video_size = video_window_.GetSize();

    if (video_size.cx != 0 && video_size.cy != 0)
    {
        DoAutoSize(video_size, true);
    }

    return 0;
}

void ViewerWindow::DoFullScreen(BOOL fullscreen)
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
                if (!CWindow(GetDesktopWindow()).GetWindowRect(&screen_rect))
                    return;
            }

            ModifyStyle(WS_CAPTION | WS_BORDER | WS_THICKFRAME | WS_MAXIMIZEBOX, WS_MAXIMIZE);
            ModifyStyleEx(0, WS_EX_TOPMOST);

            SetWindowPos(nullptr, screen_rect, SWP_SHOWWINDOW);
        }
    }
    else
    {
        ModifyStyle(WS_MAXIMIZE, WS_CAPTION | WS_BORDER | WS_THICKFRAME | WS_MAXIMIZEBOX);
        ModifyStyleEx(WS_EX_TOPMOST, 0);

        SetWindowPlacement(&window_pos_);
    }
}

LRESULT ViewerWindow::OnFullScreenButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled)
{
    DoFullScreen(toolbar_.IsButtonChecked(id));
    return 0;
}

LRESULT ViewerWindow::OnDropDownButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled)
{
    CRect rc;

    if (toolbar_.GetRect(id, &rc))
    {
        ShowDropDownMenu(id, rc);
    }

    return 0;
}

LRESULT ViewerWindow::OnPowerButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled)
{
    proto::PowerControl::Action action;
    UINT string_id;

    switch (id)
    {
        case ID_POWER_SHUTDOWN:
            action = proto::PowerControl::SHUTDOWN;
            string_id = IDS_CONF_POWER_SHUTDOWN;
            break;

        case ID_POWER_REBOOT:
            action = proto::PowerControl::REBOOT;
            string_id = IDS_CONF_POWER_REBOOT;
            break;

        case ID_POWER_HIBERNATE:
            action = proto::PowerControl::HIBERNATE;
            string_id = IDS_CONF_POWER_HIBERNATE;
            break;

        case ID_POWER_SUSPEND:
            action = proto::PowerControl::SUSPEND;
            string_id = IDS_CONF_POWER_SUSPEND;
            break;

        case ID_POWER_LOGOFF:
            action = proto::PowerControl::LOGOFF;
            string_id = IDS_CONF_POWER_LOGOFF;
            break;

        default:
            return 0;
    }

    CString title;
    title.LoadStringW(IDS_CONFIRMATION);

    CString msg;
    msg.LoadStringW(string_id);

    if (MessageBoxW(msg, title, MB_ICONQUESTION | MB_YESNO) == IDYES)
    {
        client_->SendPowerControl(action);
    }

    return 0;
}

LRESULT ViewerWindow::OnCADButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled)
{
    client_->SendKeyEvent(VK_CONTROL, proto::KeyEvent::PRESSED);
    client_->SendKeyEvent(VK_MENU, proto::KeyEvent::PRESSED);
    client_->SendKeyEvent(VK_DELETE, proto::KeyEvent::EXTENDED | proto::KeyEvent::PRESSED);

    client_->SendKeyEvent(VK_CONTROL);
    client_->SendKeyEvent(VK_MENU);
    client_->SendKeyEvent(VK_DELETE, proto::KeyEvent::EXTENDED);

    return 0;
}

LRESULT ViewerWindow::OnKeyButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled)
{
    switch (id)
    {
        case ID_KEY_CTRL_ESC:
        {
            client_->SendKeyEvent(VK_CONTROL, proto::KeyEvent::PRESSED);
            client_->SendKeyEvent(VK_ESCAPE, proto::KeyEvent::PRESSED);

            client_->SendKeyEvent(VK_CONTROL);
            client_->SendKeyEvent(VK_ESCAPE);
        }
        break;

        case ID_KEY_ALT_TAB:
        {
            client_->SendKeyEvent(VK_MENU, proto::KeyEvent::PRESSED);
            client_->SendKeyEvent(VK_TAB, proto::KeyEvent::PRESSED);

            client_->SendKeyEvent(VK_TAB);
            client_->SendKeyEvent(VK_MENU);
        }
        break;

        case ID_KEY_ALT_SHIFT_TAB:
        {
            client_->SendKeyEvent(VK_MENU, proto::KeyEvent::PRESSED);
            client_->SendKeyEvent(VK_SHIFT, proto::KeyEvent::PRESSED);
            client_->SendKeyEvent(VK_TAB, proto::KeyEvent::PRESSED);

            client_->SendKeyEvent(VK_TAB);
            client_->SendKeyEvent(VK_SHIFT);
            client_->SendKeyEvent(VK_MENU);
        }
        break;

        case ID_KEY_PRINTSCREEN:
        {
            client_->SendKeyEvent(VK_SNAPSHOT, proto::KeyEvent::PRESSED);
            client_->SendKeyEvent(VK_SNAPSHOT);
        }
        break;

        case ID_KEY_ALT_PRINTSCREEN:
        {
            client_->SendKeyEvent(VK_MENU, proto::KeyEvent::PRESSED);
            client_->SendKeyEvent(VK_SNAPSHOT, proto::KeyEvent::PRESSED);

            client_->SendKeyEvent(VK_SNAPSHOT);
            client_->SendKeyEvent(VK_MENU);
        }
        break;

        case ID_KEY_CTRL_ALT_F12:
        {
            client_->SendKeyEvent(VK_CONTROL, proto::KeyEvent::PRESSED);
            client_->SendKeyEvent(VK_MENU, proto::KeyEvent::PRESSED);
            client_->SendKeyEvent(VK_F12, proto::KeyEvent::PRESSED);

            client_->SendKeyEvent(VK_F12);
            client_->SendKeyEvent(VK_MENU);
            client_->SendKeyEvent(VK_CONTROL);
        }
        break;

        case ID_KEY_F12:
        {
            client_->SendKeyEvent(VK_F12, proto::KeyEvent::PRESSED);
            client_->SendKeyEvent(VK_F12);
        }
        break;

        case ID_KEY_CTRL_F12:
        {
            client_->SendKeyEvent(VK_CONTROL, proto::KeyEvent::PRESSED);
            client_->SendKeyEvent(VK_F12, proto::KeyEvent::PRESSED);

            client_->SendKeyEvent(VK_F12);
            client_->SendKeyEvent(VK_CONTROL);
        }
        break;
    }

    return 0;
}

LRESULT ViewerWindow::OnToolBarDropDown(int ctrl_id, LPNMHDR phdr, BOOL &handled)
{
    LPNMTOOLBARW header = reinterpret_cast<LPNMTOOLBARW>(phdr);
    ShowDropDownMenu(header->iItem, &header->rcButton);
    return 0;
}

void ViewerWindow::ShowDropDownMenu(int button_id, RECT *button_rect)
{
    UINT menu_id = -1;

    switch (button_id)
    {
        case ID_POWER:     menu_id = IDR_POWER;     break;
        case ID_SHORTCUTS: menu_id = IDR_SHORTCUTS; break;
        default: return;
    }

    if (toolbar_.MapWindowPoints(HWND_DESKTOP, button_rect))
    {
        TPMPARAMS tpm;
        tpm.cbSize = sizeof(TPMPARAMS);
        tpm.rcExclude = *button_rect;

        CMenu menu;
        menu.LoadMenuW(menu_id);

        menu.GetSubMenu(0).TrackPopupMenuEx(TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL,
                                            button_rect->left,
                                            button_rect->bottom,
                                            *this,
                                            &tpm);
    }
}

} // namespace aspia
