//
// PROJECT:         Aspia Remote Desktop
// FILE:            gui/tray_icon.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_GUI__TRAY_ICON_H
#define _ASPIA_GUI__TRAY_ICON_H

#include "aspia_config.h"

#include <shellapi.h>
#include <strsafe.h>

namespace aspia {

template <class T>
class TrayIcon
{
public:
    BEGIN_MSG_MAP(TrayIcon)
        MESSAGE_HANDLER(WM_TRAYICON, OnTrayIcon)
    END_MSG_MAP()

    TrayIcon() :
        is_installed_(false),
        default_menu_item_(0)
    {
        memset(&nid_, 0, sizeof(nid_));
        nid_.cbSize = sizeof(nid_);

        WM_TRAYICON = RegisterWindowMessageW(L"WM_TRAYICON");
    }

    ~TrayIcon()
    {
        // Remove the icon
        RemoveTrayIcon();
    }

    bool AddTrayIcon(const WCHAR *tooltip, HICON icon, UINT menu_id, UINT menu_item = 0)
    {
        T* pT = static_cast<T*>(this);

        nid_.hWnd             = pT->m_hWnd;
        nid_.uID              = menu_id;
        nid_.hIcon            = icon;
        nid_.uFlags           = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        nid_.uCallbackMessage = WM_TRAYICON;

        StringCbCopyW(nid_.szTip, sizeof(nid_.szTip), tooltip);

        is_installed_ = !!Shell_NotifyIconW(NIM_ADD, &nid_);

        default_menu_item_ = menu_item;

        return is_installed_;
    }

    bool AddTrayIcon(const WCHAR *tooltip, UINT icon_id, UINT menu_id, UINT menu_item = 0)
    {
        CIcon icon(AtlLoadIconImage(icon_id,
                                    LR_DEFAULTCOLOR | LR_DEFAULTSIZE,
                                    GetSystemMetrics(SM_CXSMICON),
                                    GetSystemMetrics(SM_CYSMICON)));
        return AddTrayIcon(tooltip, icon, menu_id, menu_item);
    }

    bool RemoveTrayIcon()
    {
        if (!is_installed_) return false;

        nid_.uFlags = 0;

        return !!Shell_NotifyIconW(NIM_DELETE, &nid_);
    }

    // Set the default menu item ID
    inline void UpdateTrayDefaultMenuItem(UINT menu_item)
    {
        default_menu_item_ = menu_item;
    }

    bool SetTrayIconTooltip(const WCHAR *tooltip)
    {
        if (!tooltip) return false;

        nid_.uFlags = NIF_TIP;
        StringCbCopyW(nid_.szTip, sizeof(nid_.szTip), tooltip);

        return !!Shell_NotifyIconW(NIM_MODIFY, &nid_);
    }

    bool SetTrayIcon(HICON icon)
    {
        if (!icon) return false;

        nid_.uFlags = NIF_ICON;
        nid_.hIcon = icon;

        return !!Shell_NotifyIconW(NIM_MODIFY, &nid_);
    }

    bool SetTrayIcon(UINT icon_id)
    {
        CIcon icon(AtlLoadIconImage(icon_id,
                                    LR_DEFAULTCOLOR | LR_DEFAULTSIZE,
                                    GetSystemMetrics(SM_CXSMICON),
                                    GetSystemMetrics(SM_CYSMICON)));
        return SetTrayIcon(icon);
    }

    // Allow the menu items to be enabled/checked/etc.
    virtual void PrepareMenu(HMENU menu)
    {
        UNREFERENCED_PARAMETER(menu);
    }

private:
    LRESULT OnTrayIcon(UINT msg, WPARAM wParam, LPARAM lParam, BOOL &handled)
    {
        // Is this the ID we want?
        if (wParam != nid_.uID)
            return 0;

        T* pT = static_cast<T*>(this);

        // Was the right-button clicked?
        if (LOWORD(lParam) == WM_RBUTTONUP)
        {
            // Load the menu
            CMenu menu;
            if (!menu.LoadMenuW(nid_.uID))
                return 0;

            // Get the sub-menu
            CMenuHandle popup(menu.GetSubMenu(0));

            // Prepare
            pT->PrepareMenu(popup);

            // Get the menu position
            CPoint pos;
            GetCursorPos(&pos);

            // Make app the foreground
            SetForegroundWindow(pT->m_hWnd);

            // Set the default menu item
            if (!default_menu_item_)
                popup.SetMenuDefaultItem(0, TRUE);
            else
                popup.SetMenuDefaultItem(default_menu_item_);

            popup.TrackPopupMenu(TPM_LEFTALIGN, pos.x, pos.y, pT->m_hWnd);

            pT->PostMessageW(WM_NULL);
        }
        else if (LOWORD(lParam) == WM_LBUTTONDBLCLK)
        {
            pT->SendMessageW(WM_COMMAND, default_menu_item_, 0);
        }

        return 0;
    }

private:
    UINT WM_TRAYICON;
    NOTIFYICONDATAW nid_;
    bool is_installed_;
    UINT default_menu_item_;
};

} // namespace aspia

#endif // _ASPIA_GUI__TRAY_ICON_H
