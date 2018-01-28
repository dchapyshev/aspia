//
// PROJECT:         Aspia
// FILE:            ui/base/tray_icon.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__BASE__TRAY_ICON_H
#define _ASPIA_UI__BASE__TRAY_ICON_H

#include "base/macros.h"
#include "base/logging.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlmisc.h>
#include <shellapi.h>
#include <strsafe.h>
#include <string>

namespace aspia {

template <class T>
class TrayIcon
{
public:
    TrayIcon()
    {
        memset(&nid_, 0, sizeof(nid_));
        nid_.cbSize = sizeof(nid_);

        message_id_ = RegisterWindowMessageW(L"WM_TRAYICON");
        if (!message_id_)
        {
            DPLOG(LS_ERROR) << "RegisterWindowMessageW failed";
        }
    }

    ~TrayIcon()
    {
        RemoveTrayIcon();
    }

    BOOL AddTrayIcon(HICON icon, const WCHAR* tooltip, UINT menu_id)
    {
        T* pT = static_cast<T*>(this);

        nid_.hWnd   = pT->m_hWnd;
        nid_.uID    = menu_id;
        nid_.hIcon  = icon;
        nid_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        nid_.uCallbackMessage = message_id_;

        HRESULT hr = StringCbCopyW(nid_.szTip, sizeof(nid_.szTip), tooltip);
        if (FAILED(hr))
        {
            DLOG(LS_ERROR) << "StringCbCopyW failed: " << SystemErrorCodeToString(hr);
            return FALSE;
        }

        is_installed_ = Shell_NotifyIconW(NIM_ADD, &nid_);
        return is_installed_;
    }

    BOOL RemoveTrayIcon()
    {
        if (!is_installed_)
            return FALSE;

        nid_.uFlags = 0;
        return Shell_NotifyIconW(NIM_DELETE, &nid_);
    }

    void SetDefaultTrayMenuItem(UINT menu_item)
    {
        default_menu_item_ = menu_item;
    }

    BOOL SetTrayTooltip(const WCHAR *tooltip)
    {
        if (!tooltip)
            return FALSE;

        nid_.uFlags = NIF_TIP;

        HRESULT hr = StringCbCopyW(nid_.szTip, sizeof(nid_.szTip), tooltip);
        if (FAILED(hr))
        {
            DLOG(ERROR) << "StringCbCopyW failed: " << SystemErrorCodeToString(hr);
            return false;
        }

        return Shell_NotifyIconW(NIM_MODIFY, &nid_);
    }

    BOOL SetTrayIcon(HICON icon)
    {
        if (!icon)
            return FALSE;

        nid_.uFlags = NIF_ICON;
        nid_.hIcon  = hIcon;

        return Shell_NotifyIconW(NIM_MODIFY, &nid_);
    }

    BOOL SetTrayIcon(UINT icon_id)
    {
        CIcon icon = AtlLoadIconImage(icon_id,
                                      LR_DEFAULTCOLOR | LR_DEFAULTSIZE,
                                      GetSystemMetrics(SM_CXSMICON),
                                      GetSystemMetrics(SM_CYSMICON));
        return SetIcon(icon);
    }

private:
    BEGIN_MSG_MAP(TrayIcon)
        MESSAGE_HANDLER(message_id_, OnTrayIcon)
    END_MSG_MAP()

    LRESULT OnTrayIcon(UINT /* msg */, WPARAM wparam, LPARAM lparam, BOOL& /* handled */)
    {
        // Is this the ID we want?
        if (wparam != nid_.uID)
            return 0;

        T* pT = static_cast<T*>(this);

        // Was the right-button clicked?
        if (LOWORD(lparam) == WM_RBUTTONUP)
        {
            // Load the menu
            CMenu menu;
            if (!menu.LoadMenuW(nid_.uID))
                return 0;

            CMenuHandle popup(menu.GetSubMenu(0));

            // Get the menu position
            CPoint pos;
            ::GetCursorPos(&pos);

            ::SetForegroundWindow(pT->m_hWnd);

            if (default_menu_item_ == 0)
                popup.SetMenuDefaultItem(0, TRUE);
            else
                popup.SetMenuDefaultItem(default_menu_item_);

            popup.TrackPopupMenu(TPM_LEFTALIGN, pos.x, pos.y, pT->m_hWnd);

            pT->PostMessageW(WM_NULL);
        }
        else if (LOWORD(lparam) == WM_LBUTTONDBLCLK)
        {
            pT->SendMessageW(WM_COMMAND, default_menu_item_, 0);
        }
        return 0;
    }

    UINT message_id_;
    NOTIFYICONDATAW nid_;
    BOOL is_installed_ = FALSE;
    UINT default_menu_item_ = 0;

    DISALLOW_COPY_AND_ASSIGN(TrayIcon);
};

} // namespace aspia

#endif // _ASPIA_UI__BASE__TRAY_ICON_H
