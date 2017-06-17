//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/tray_icon.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__BASE__TRAY_ICON_H
#define _ASPIA_UI__BASE__TRAY_ICON_H

#include "base/macros.h"

#include <shellapi.h>
#include <string>

namespace aspia {

class UiTrayIcon
{
public:
    UiTrayIcon();
    ~UiTrayIcon();

    bool AddIcon(HWND hwnd, HICON icon, const std::wstring& tooltip, UINT menu_id);
    bool AddIcon(HWND hwnd, UINT icon_id, const std::wstring& tooltip, UINT menu_id);
    bool RemoveIcon();
    void SetDefaultMenuItem(UINT menu_item);
    bool SetTooltip(const WCHAR *tooltip);
    bool SetIcon(HICON icon);
    bool SetIcon(UINT icon_id);

    bool HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam);

private:
    HWND hwnd_ = nullptr;
    UINT message_id_;
    NOTIFYICONDATAW nid_;
    bool is_installed_ = false;
    UINT default_menu_item_ = 0;

    DISALLOW_COPY_AND_ASSIGN(UiTrayIcon);
};

} // namespace aspia

#endif // _ASPIA_UI__BASE__TRAY_ICON_H
