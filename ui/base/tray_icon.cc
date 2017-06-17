//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/tray_icon.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/base/tray_icon.h"
#include "ui/base/module.h"
#include "base/scoped_user_object.h"
#include "base/logging.h"

#include <strsafe.h>

namespace aspia {

UiTrayIcon::UiTrayIcon()
{
    memset(&nid_, 0, sizeof(nid_));
    nid_.cbSize = sizeof(nid_);

    message_id_ = RegisterWindowMessageW(L"WM_TRAYICON");
    if (!message_id_)
    {
        DLOG(ERROR) << "RegisterWindowMessageW() failed: "
                    << GetLastSystemErrorString();
    }
}

UiTrayIcon::~UiTrayIcon()
{
    RemoveIcon();
}

bool UiTrayIcon::AddIcon(HWND hwnd, HICON icon, const std::wstring& tooltip, UINT menu_id)
{
    hwnd_ = hwnd;

    nid_.hWnd   = hwnd;
    nid_.uID    = menu_id;
    nid_.hIcon  = icon;
    nid_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid_.uCallbackMessage = message_id_;

    HRESULT hr = StringCbCopyW(nid_.szTip, sizeof(nid_.szTip), tooltip.c_str());
    if (FAILED(hr))
    {
        DLOG(ERROR) << "StringCbCopyW() failed: " << SystemErrorCodeToString(hr);
        return false;
    }

    is_installed_ = !!Shell_NotifyIconW(NIM_ADD, &nid_);

    default_menu_item_ = 0;

    return is_installed_;
}

bool UiTrayIcon::AddIcon(HWND hwnd, UINT icon_id, const std::wstring& tooltip, UINT menu_id)
{
    ScopedHICON icon(UiModule::Current().icon(icon_id,
                                              GetSystemMetrics(SM_CXSMICON),
                                              GetSystemMetrics(SM_CYSMICON),
                                              LR_DEFAULTCOLOR | LR_DEFAULTSIZE));
    return AddIcon(hwnd, icon, tooltip, menu_id);
}

bool UiTrayIcon::RemoveIcon()
{
    if (!is_installed_)
        return false;

    nid_.uFlags = 0;

    return !!Shell_NotifyIconW(NIM_DELETE, &nid_);
}

void UiTrayIcon::SetDefaultMenuItem(UINT menu_item)
{
    default_menu_item_ = menu_item;
}

bool UiTrayIcon::SetTooltip(const WCHAR *tooltip)
{
    if (!tooltip)
        return false;

    nid_.uFlags = NIF_TIP;

    HRESULT hr = StringCbCopyW(nid_.szTip, sizeof(nid_.szTip), tooltip);
    if (FAILED(hr))
    {
        DLOG(ERROR) << "StringCbCopyW() failed: " << SystemErrorCodeToString(hr);
        return false;
    }

    return !!Shell_NotifyIconW(NIM_MODIFY, &nid_);
}

bool UiTrayIcon::SetIcon(HICON icon)
{
    if (!icon)
        return false;

    nid_.uFlags = NIF_ICON;
    nid_.hIcon = icon;

    return !!Shell_NotifyIconW(NIM_MODIFY, &nid_);
}

bool UiTrayIcon::SetIcon(UINT icon_id)
{
    ScopedHICON icon(UiModule::Current().icon(icon_id,
                                              GetSystemMetrics(SM_CXSMICON),
                                              GetSystemMetrics(SM_CYSMICON),
                                              LR_DEFAULTCOLOR | LR_DEFAULTSIZE));
    return SetIcon(icon);
}

bool UiTrayIcon::HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (msg != message_id_ || wparam != nid_.uID)
        return false;

    if (LOWORD(lparam) == WM_RBUTTONUP)
    {
        ScopedHMENU menu(UiModule::Current().menu(nid_.uID));

        HMENU popup_menu = GetSubMenu(menu, 0);

        POINT pos;
        GetCursorPos(&pos);

        SetForegroundWindow(hwnd_);

        if (!default_menu_item_)
            SetMenuDefaultItem(popup_menu, 0, TRUE);
        else
            SetMenuDefaultItem(popup_menu, default_menu_item_, FALSE);

        TrackPopupMenu(popup_menu, TPM_LEFTALIGN, pos.x, pos.y, 0, hwnd_, nullptr);

        PostMessageW(hwnd_, WM_NULL, 0, 0);
    }
    else if (LOWORD(lparam) == WM_LBUTTONDBLCLK)
    {
        SendMessageW(hwnd_, WM_COMMAND, default_menu_item_, 0);
    }

    return 0;
}

} // namespace aspia
