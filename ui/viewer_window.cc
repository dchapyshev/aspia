//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/viewer_window.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/viewer_window.h"
#include "ui/power_manage_dialog.h"
#include "ui/resource.h"
#include "ui/base/module.h"
#include "base/unicode.h"
#include "desktop_capture/cursor.h"
#include "proto/desktop_session_message.pb.h"

namespace aspia {

static const DWORD kKeyUpFlag = 0x80000000;
static const DWORD kKeyExtendedFlag = 0x1000000;

static const DesktopSize kVideoWindowSize(400, 280);

ViewerWindow::ViewerWindow(const ClientConfig& config, Delegate* delegate) :
    config_(config),
    delegate_(delegate),
    video_window_(this)
{
    memset(&window_pos_, 0, sizeof(window_pos_));

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

    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
        WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;

    if (!Create(nullptr, style))
    {
        LOG(ERROR) << "Viewer window not created";
        runner_->PostQuit();
    }
    else
    {
        ScopedHICON icon(Module::Current().icon(IDI_MAIN,
                                                GetSystemMetrics(SM_CXSMICON),
                                                GetSystemMetrics(SM_CYSMICON),
                                                LR_CREATEDIBSECTION));
        SetIcon(icon);
        SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    }
}

void ViewerWindow::OnAfterThreadRunning()
{
    DestroyWindow(hwnd());
}

DesktopFrame* ViewerWindow::Frame()
{
    return video_window_.Frame();
}

void ViewerWindow::DrawFrame()
{
    video_window_.DrawFrame();
}

void ViewerWindow::ResizeFrame(const DesktopSize& size, const PixelFormat& format)
{
    ShowScrollBar(video_window_, SB_BOTH, FALSE);

    int sb = -1;

    if (!size.IsEqual(video_window_.FrameSize()))
        sb = DoAutoSize(size);

    video_window_.ResizeFrame(size, format);

    if (sb != -1)
        ShowScrollBar(video_window_, sb, FALSE);
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

    // If we can not get a remote cursor, then we use the default cursor.
    if (!cursor.IsValid())
    {
        SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    }
    else
    {
        SetCursor(cursor);
    }
}

void ViewerWindow::InjectClipboardEvent(std::shared_ptr<proto::ClipboardEvent> clipboard_event)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&ViewerWindow::InjectClipboardEvent, this, clipboard_event));
        return;
    }

    clipboard_.InjectClipboardEvent(clipboard_event);
}

void ViewerWindow::CreateToolBar()
{
    toolbar_.Attach(CreateWindowExW(0,
                                    TOOLBARCLASSNAME,
                                    nullptr,
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBSTYLE_FLAT |
                                        TBSTYLE_TOOLTIPS,
                                    CW_USEDEFAULT, CW_USEDEFAULT,
                                    CW_USEDEFAULT, CW_USEDEFAULT,
                                    hwnd(),
                                    nullptr,
                                    Module().Current().Handle(),
                                    nullptr));

    DWORD extended_style = SendMessageW(toolbar_, TB_GETEXTENDEDSTYLE, 0, 0);

    extended_style |= TBSTYLE_EX_DRAWDDARROWS;

    SendMessageW(toolbar_, TB_SETEXTENDEDSTYLE, 0, extended_style);

    TBBUTTON kButtons[] =
    {
        // iBitmap, idCommand, fsState, fsStyle, bReserved[2], dwData, iString
        {  0, ID_POWER,      TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE,                { 0 }, 0, -1 },
        { -1, 0,             TBSTATE_ENABLED, BTNS_SEP,                                   { 0 }, 0, -1 },
        {  1, ID_CAD,        TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE,                { 0 }, 0, -1 },
        {  2, ID_SHORTCUTS,  TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_DROPDOWN,{ 0 }, 0, -1 },
        { -1, 0,             TBSTATE_ENABLED, BTNS_SEP,                                   { 0 }, 0, -1 },
        {  3, ID_AUTO_SIZE,  TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE,                { 0 }, 0, -1 },
        {  4, ID_FULLSCREEN, TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_CHECK,   { 0 }, 0, -1 },
        { -1, 0,             TBSTATE_ENABLED, BTNS_SEP,                                   { 0 }, 0, -1 },
        {  5, ID_SETTINGS,   TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE,                { 0 }, 0, -1 },
        { -1, 0,             TBSTATE_ENABLED, BTNS_SEP,                                   { 0 }, 0, -1 },
        {  6, ID_ABOUT,      TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE,                { 0 }, 0, -1 },
        {  7, ID_EXIT,       TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE,                { 0 }, 0, -1 },
    };

    SendMessageW(toolbar_, TB_BUTTONSTRUCTSIZE, sizeof(kButtons[0]), 0);

    SendMessageW(toolbar_,
                 TB_ADDBUTTONS,
                 _countof(kButtons),
                 reinterpret_cast<LPARAM>(kButtons));

    if (toolbar_imagelist_.CreateSmall())
    {
        const Module& module = Module().Current();

        toolbar_imagelist_.AddIcon(module, IDI_POWER);
        toolbar_imagelist_.AddIcon(module, IDI_CAD);
        toolbar_imagelist_.AddIcon(module, IDI_KEYS);
        toolbar_imagelist_.AddIcon(module, IDI_AUTOSIZE);
        toolbar_imagelist_.AddIcon(module, IDI_FULLSCREEN);
        toolbar_imagelist_.AddIcon(module, IDI_SETTINGS);
        toolbar_imagelist_.AddIcon(module, IDI_ABOUT);
        toolbar_imagelist_.AddIcon(module, IDI_EXIT);

        SendMessageW(toolbar_,
                     TB_SETIMAGELIST,
                     0,
                     reinterpret_cast<LPARAM>(toolbar_imagelist_.Handle()));
    }

    if (config_.session_type() == proto::SessionType::SESSION_TYPE_DESKTOP_VIEW)
    {
        TBBUTTONINFOW button_info;
        button_info.cbSize  = sizeof(button_info);
        button_info.dwMask  = TBIF_STATE;
        button_info.fsState = 0;

        SendMessageW(toolbar_, TB_SETBUTTONINFOW, ID_POWER, reinterpret_cast<LPARAM>(&button_info));
        SendMessageW(toolbar_, TB_SETBUTTONINFOW, ID_CAD, reinterpret_cast<LPARAM>(&button_info));
        SendMessageW(toolbar_, TB_SETBUTTONINFOW, ID_SHORTCUTS, reinterpret_cast<LPARAM>(&button_info));
    }
}

void ViewerWindow::OnCreate()
{
    std::wstring title(config_.address());

    title.append(L" - ");
    title.append(Module().Current().string(IDS_APPLICATION_NAME));

    SetWindowTextW(hwnd(), title.c_str());

    CreateToolBar();
    video_window_.Create(hwnd(), WS_CHILD);

    DoAutoSize(kVideoWindowSize);

    ApplyConfig(config_.desktop_session_config());
}

void ViewerWindow::OnClose()
{
    delegate_->OnWindowClose();
    DestroyWindow(video_window_);
}

void ViewerWindow::OnSize()
{
    SendMessageW(toolbar_, TB_AUTOSIZE, 0, 0);

    RECT rc = { 0 };
    GetWindowRect(toolbar_, &rc);

    int toolbar_height = rc.bottom - rc.top;

    GetClientRect(hwnd(), &rc);

    MoveWindow(video_window_,
               0,
               toolbar_height,
               rc.right - rc.left,
               rc.bottom - rc.top - toolbar_height,
               TRUE);
}

void ViewerWindow::OnGetMinMaxInfo(LPMINMAXINFO mmi)
{
    mmi->ptMinTrackSize.x = kVideoWindowSize.Width();
    mmi->ptMinTrackSize.y = kVideoWindowSize.Height();
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

                    if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
                    {
                        // Bit 31 set to 1 means that the key is not pressed.
                        flags |= kKeyUpFlag;
                    }

                    // Send the message to the window that is in focus.
                    SendMessageW(gui_info.hwndFocus, wParam, hook_struct->vkCode, flags);

                    return TRUE;
                }
                break;
            }
        }
    }

    return CallNextHookEx(nullptr, code, wParam, lParam);
}

void ViewerWindow::OnActivate(UINT state)
{
    if (state == WA_ACTIVE || state == WA_CLICKACTIVE)
    {
        // To properly handle some key combinations, we need to set a hook
        // that will prevent the system processing these presses.
        // These combinations include Left Win, Right Win, Alt + Tab.
        keyboard_hook_.Reset(SetWindowsHookExW(WH_KEYBOARD_LL,
                                               KeyboardHookProc,
                                               GetModuleHandleW(nullptr),
                                               0));
        video_window_.HasFocus(true);
    }
    else if (state == WA_INACTIVE)
    {
        keyboard_hook_.Reset();
        video_window_.HasFocus(false);
    }
}

void ViewerWindow::OnKeyboard(WPARAM wParam, LPARAM lParam)
{
    uint8_t key_code = static_cast<uint8_t>(static_cast<uint32_t>(wParam) & 255);

    // We do not pass CapsLock and NoLock directly. Instead, when sending each
    // keystroke event, a flag is set that indicates the status of these keys.
    if (key_code != VK_CAPITAL && key_code != VK_NUMLOCK)
    {
        uint32_t key_data = static_cast<uint32_t>(lParam);

        uint32_t flags = 0;

        flags |= ((key_data & kKeyUpFlag) == 0) ? proto::KeyEvent::PRESSED : 0;
        flags |= ((key_data & kKeyExtendedFlag) != 0) ? proto::KeyEvent::EXTENDED : 0;
        flags |= (GetKeyState(VK_CAPITAL) != 0) ? proto::KeyEvent::CAPSLOCK : 0;
        flags |= (GetKeyState(VK_NUMLOCK) != 0) ? proto::KeyEvent::NUMLOCK : 0;

        delegate_->OnKeyEvent(key_code, flags);
    }
}

void ViewerWindow::OnSetFocus()
{
    video_window_.HasFocus(true);
}

void ViewerWindow::OnKillFocus()
{
    video_window_.HasFocus(false);
}

void ViewerWindow::ApplyConfig(const proto::DesktopSessionConfig& config)
{
    if (!(config.flags() & proto::DesktopSessionConfig::ENABLE_CURSOR_SHAPE))
    {
        SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    }

    if (config.flags() & proto::DesktopSessionConfig::ENABLE_CLIPBOARD)
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

void ViewerWindow::OnSettingsButton()
{
    SettingsDialog dialog;

    if (dialog.DoModal(hwnd(),
                       config_.session_type(),
                       config_.desktop_session_config()) == IDOK)
    {
        config_.mutable_desktop_session_config()->CopyFrom(dialog.Config());

        ApplyConfig(config_.desktop_session_config());
        delegate_->OnConfigChange(config_.desktop_session_config());
    }
}

void ViewerWindow::OnAboutButton()
{
    AboutDialog dialog;
    dialog.DoModal(hwnd());
}

void ViewerWindow::OnExitButton()
{
    PostMessageW(hwnd(), WM_CLOSE, 0, 0);
}

int ViewerWindow::DoAutoSize(const DesktopSize &video_frame_size)
{
    if (SendMessageW(toolbar_, TB_ISBUTTONCHECKED, ID_FULLSCREEN, 0))
        DoFullScreen(false);

    RECT screen_rect = { 0 };

    HMONITOR monitor = MonitorFromWindow(hwnd(), MONITOR_DEFAULTTONEAREST);

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
            if (!::GetClientRect(GetDesktopWindow(), &screen_rect))
                return -1;
        }
    }

    WINDOWPLACEMENT wp = { 0 };

    wp.length = sizeof(wp);

    if (!GetWindowPlacement(hwnd(), &wp))
        return -1;

    RECT full_rect = { 0 };
    GetWindowRect(hwnd(), &full_rect);

    RECT client_rect = { 0 };
    GetClientRect(hwnd(), &client_rect);

    RECT toolbar_rect = { 0 };
    GetWindowRect(toolbar_, &toolbar_rect);

    int client_area_width = video_frame_size.Width() +
        (full_rect.right - full_rect.left) - (client_rect.right - client_rect.left);

    int client_area_height = video_frame_size.Height() +
        (full_rect.bottom - full_rect.top) - (client_rect.bottom - client_rect.top) +
        (toolbar_rect.bottom - toolbar_rect.top);

    int screen_width = screen_rect.right - screen_rect.left;
    int screen_height = screen_rect.bottom - screen_rect.top;

    if (client_area_width < screen_width && client_area_height < screen_height)
    {
        if (wp.showCmd != SW_MAXIMIZE)
        {
            SetWindowPos(hwnd(),
                         nullptr,
                         (screen_width - client_area_width) / 2,
                         (screen_height - client_area_height) / 2,
                         client_area_width,
                         client_area_height,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }

        return SB_BOTH;
    }
    else
    {
        if (wp.showCmd != SW_MAXIMIZE)
            ShowWindow(hwnd(), SW_MAXIMIZE);

        if (client_area_width < screen_width)
            return SB_HORZ;
        else if (client_area_height < screen_height)
            return SB_VERT;
    }

    return -1;
}

void ViewerWindow::OnAutoSizeButton()
{
    DesktopSize video_frame_size = video_window_.FrameSize();

    if (!video_frame_size.IsEmpty())
    {
        if (DoAutoSize(video_frame_size) != -1)
            ShowScrollBar(video_window_, SB_BOTH, FALSE);
    }
}

void ViewerWindow::DoFullScreen(bool fullscreen)
{
    SendMessageW(toolbar_, TB_CHECKBUTTON, ID_FULLSCREEN, MAKELPARAM(fullscreen, 0));

    if (fullscreen)
    {
        if (GetWindowPlacement(hwnd(), &window_pos_))
        {
            RECT screen_rect = { 0 };

            HMONITOR monitor = MonitorFromWindow(hwnd(), MONITOR_DEFAULTTONEAREST);

            MONITORINFO info;
            info.cbSize = sizeof(info);

            if (GetMonitorInfoW(monitor, &info))
            {
                screen_rect = info.rcMonitor;
            }
            else
            {
                if (!GetWindowRect(GetDesktopWindow(), &screen_rect))
                    return;
            }

            ModifyStyle(WS_CAPTION | WS_BORDER | WS_THICKFRAME | WS_MAXIMIZEBOX, WS_MAXIMIZE);
            ModifyStyleEx(0, WS_EX_TOPMOST);

            SetWindowPos(hwnd(),
                         nullptr,
                         screen_rect.left,
                         screen_rect.top,
                         screen_rect.right - screen_rect.left,
                         screen_rect.bottom - screen_rect.top,
                         SWP_SHOWWINDOW);
        }
    }
    else
    {
        ModifyStyle(WS_MAXIMIZE, WS_CAPTION | WS_BORDER | WS_THICKFRAME | WS_MAXIMIZEBOX);
        ModifyStyleEx(WS_EX_TOPMOST, 0);

        SetWindowPlacement(hwnd(), &window_pos_);
    }
}

void ViewerWindow::OnFullScreenButton()
{
    DoFullScreen(!!SendMessageW(toolbar_, TB_ISBUTTONCHECKED, ID_FULLSCREEN, 0));
}

void ViewerWindow::OnDropDownButton(WORD ctrl_id)
{
    RECT rc = { 0 };
    SendMessageW(toolbar_, TB_GETRECT, ctrl_id, reinterpret_cast<LPARAM>(&rc));

    ShowDropDownMenu(ctrl_id, &rc);
}

void ViewerWindow::OnPowerButton()
{
    PowerManageDialog dialog;

    proto::PowerEvent::Action action =
        static_cast<proto::PowerEvent::Action>(dialog.DoModal(hwnd()));

    if (action != proto::PowerEvent::UNKNOWN)
        delegate_->OnPowerEvent(action);
}

void ViewerWindow::OnCADButton()
{
    delegate_->OnKeyEvent(VK_CONTROL, proto::KeyEvent::PRESSED);
    delegate_->OnKeyEvent(VK_MENU, proto::KeyEvent::PRESSED);
    delegate_->OnKeyEvent(VK_DELETE, proto::KeyEvent::EXTENDED | proto::KeyEvent::PRESSED);

    delegate_->OnKeyEvent(VK_CONTROL, 0);
    delegate_->OnKeyEvent(VK_MENU, 0);
    delegate_->OnKeyEvent(VK_DELETE, proto::KeyEvent::EXTENDED);
}

void ViewerWindow::OnKeyButton(WORD ctrl_id)
{
    switch (ctrl_id)
    {
        case ID_KEY_CTRL_ESC:
        {
            delegate_->OnKeyEvent(VK_CONTROL, proto::KeyEvent::PRESSED);
            delegate_->OnKeyEvent(VK_ESCAPE, proto::KeyEvent::PRESSED);

            delegate_->OnKeyEvent(VK_CONTROL, 0);
            delegate_->OnKeyEvent(VK_ESCAPE, 0);
        }
        break;

        case ID_KEY_ALT_TAB:
        {
            delegate_->OnKeyEvent(VK_MENU, proto::KeyEvent::PRESSED);
            delegate_->OnKeyEvent(VK_TAB, proto::KeyEvent::PRESSED);

            delegate_->OnKeyEvent(VK_TAB, 0);
            delegate_->OnKeyEvent(VK_MENU, 0);
        }
        break;

        case ID_KEY_ALT_SHIFT_TAB:
        {
            delegate_->OnKeyEvent(VK_MENU, proto::KeyEvent::PRESSED);
            delegate_->OnKeyEvent(VK_SHIFT, proto::KeyEvent::PRESSED);
            delegate_->OnKeyEvent(VK_TAB, proto::KeyEvent::PRESSED);

            delegate_->OnKeyEvent(VK_TAB, 0);
            delegate_->OnKeyEvent(VK_SHIFT, 0);
            delegate_->OnKeyEvent(VK_MENU, 0);
        }
        break;

        case ID_KEY_PRINTSCREEN:
        {
            delegate_->OnKeyEvent(VK_SNAPSHOT, proto::KeyEvent::PRESSED);
            delegate_->OnKeyEvent(VK_SNAPSHOT, 0);
        }
        break;

        case ID_KEY_ALT_PRINTSCREEN:
        {
            delegate_->OnKeyEvent(VK_MENU, proto::KeyEvent::PRESSED);
            delegate_->OnKeyEvent(VK_SNAPSHOT, proto::KeyEvent::PRESSED);

            delegate_->OnKeyEvent(VK_SNAPSHOT, 0);
            delegate_->OnKeyEvent(VK_MENU, 0);
        }
        break;

        case ID_KEY_CTRL_ALT_F12:
        {
            delegate_->OnKeyEvent(VK_CONTROL, proto::KeyEvent::PRESSED);
            delegate_->OnKeyEvent(VK_MENU, proto::KeyEvent::PRESSED);
            delegate_->OnKeyEvent(VK_F12, proto::KeyEvent::PRESSED);

            delegate_->OnKeyEvent(VK_F12, 0);
            delegate_->OnKeyEvent(VK_MENU, 0);
            delegate_->OnKeyEvent(VK_CONTROL, 0);
        }
        break;

        case ID_KEY_F12:
        {
            delegate_->OnKeyEvent(VK_F12, proto::KeyEvent::PRESSED);
            delegate_->OnKeyEvent(VK_F12, 0);
        }
        break;

        case ID_KEY_CTRL_F12:
        {
            delegate_->OnKeyEvent(VK_CONTROL, proto::KeyEvent::PRESSED);
            delegate_->OnKeyEvent(VK_F12, proto::KeyEvent::PRESSED);

            delegate_->OnKeyEvent(VK_F12, 0);
            delegate_->OnKeyEvent(VK_CONTROL, 0);
        }
        break;
    }
}

void ViewerWindow::OnGetDispInfo(LPNMHDR phdr)
{
    LPTOOLTIPTEXTW header = reinterpret_cast<LPTOOLTIPTEXTW>(phdr);
    UINT string_id;

    switch (header->hdr.idFrom)
    {
        case ID_POWER:
            string_id = IDS_POWER;
            break;

        case ID_ABOUT:
            string_id = IDS_ABOUT;
            break;

        case ID_AUTO_SIZE:
            string_id = IDS_AUTO_SIZE;
            break;

        case ID_FULLSCREEN:
            string_id = IDS_FULLSCREEN;
            break;

        case ID_EXIT:
            string_id = IDS_EXIT;
            break;

        case ID_CAD:
            string_id = IDS_CAD;
            break;

        case ID_SHORTCUTS:
            string_id = IDS_SHORTCUTS;
            break;

        case ID_SETTINGS:
            string_id = IDS_SETTINGS;
            break;

        default:
            return;
    }

    LoadStringW(Module().Current().Handle(),
                string_id,
                header->szText,
                _countof(header->szText));
}

void ViewerWindow::OnToolBarDropDown(LPNMHDR phdr)
{
    LPNMTOOLBARW header = reinterpret_cast<LPNMTOOLBARW>(phdr);
    ShowDropDownMenu(header->iItem, &header->rcButton);
}

void ViewerWindow::ShowDropDownMenu(int button_id, RECT* button_rect)
{
    if (button_id != ID_SHORTCUTS)
        return;

    if (MapWindowPoints(toolbar_, HWND_DESKTOP, reinterpret_cast<LPPOINT>(button_rect), 2))
    {
        TPMPARAMS tpm;
        tpm.cbSize = sizeof(TPMPARAMS);
        tpm.rcExclude = *button_rect;

        ScopedHMENU menu(Module().Current().menu(IDR_SHORTCUTS));

        TrackPopupMenuEx(GetSubMenu(menu, 0),
                         TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL,
                         button_rect->left,
                         button_rect->bottom,
                         hwnd(),
                         &tpm);
    }
}

bool ViewerWindow::OnMessage(UINT msg, WPARAM wparam, LPARAM lparam, LRESULT* result)
{
    switch (msg)
    {
        case WM_CREATE:
            OnCreate();
            break;

        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
            OnKeyboard(wparam, lparam);
            break;

        case WM_CHAR:
        case WM_SYSCHAR:
        case WM_DEADCHAR:
        case WM_SYSDEADCHAR:
            break;

        case WM_MOUSEWHEEL:
            video_window_.OnMouse(msg, wparam, lparam);
            break;

        case WM_SIZE:
            OnSize();
            break;

        case WM_GETMINMAXINFO:
            OnGetMinMaxInfo(reinterpret_cast<LPMINMAXINFO>(lparam));
            break;

        case WM_ACTIVATE:
            OnActivate(LOWORD(wparam));
            break;

        case WM_SETFOCUS:
            OnSetFocus();
            break;

        case WM_KILLFOCUS:
            OnKillFocus();
            break;

        case WM_NOTIFY:
        {
            LPNMHDR header = reinterpret_cast<LPNMHDR>(lparam);

            switch (header->code)
            {
                case TTN_GETDISPINFO:
                    OnGetDispInfo(header);
                    break;

                case TBN_DROPDOWN:
                    OnToolBarDropDown(header);
                    break;
            }
        }
        break;

        case WM_COMMAND:
        {
            switch (LOWORD(wparam))
            {
                case ID_SETTINGS:
                    OnSettingsButton();
                    break;

                case ID_ABOUT:
                    OnAboutButton();
                    break;

                case ID_EXIT:
                    OnExitButton();
                    break;

                case ID_AUTO_SIZE:
                    OnAutoSizeButton();
                    break;

                case ID_FULLSCREEN:
                    OnFullScreenButton();
                    break;

                case ID_SHORTCUTS:
                    OnDropDownButton(LOWORD(wparam));
                    break;

                case ID_CAD:
                    OnCADButton();
                    break;

                case ID_KEY_CTRL_ESC:
                case ID_KEY_ALT_TAB:
                case ID_KEY_ALT_SHIFT_TAB:
                case ID_KEY_PRINTSCREEN:
                case ID_KEY_ALT_PRINTSCREEN:
                case ID_KEY_CTRL_ALT_F12:
                case ID_KEY_F12:
                case ID_KEY_CTRL_F12:
                    OnKeyButton(LOWORD(wparam));
                    break;

                case ID_POWER:
                    OnPowerButton();
                    break;
            }
        }
        break;

        case WM_CLOSE:
            OnClose();
            break;
    }

    return 0;
}

void ViewerWindow::OnPointerEvent(int32_t x, int32_t y, uint32_t mask)
{
    delegate_->OnPointerEvent(x, y, mask);
}

} // namespace aspia
