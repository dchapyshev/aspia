//
// PROJECT:         Aspia Remote Desktop
// FILE:            gui/add_user_dialog.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "gui/add_user_dialog.h"

namespace aspia {

BOOL AddUserDialog::OnInitDialog(CWindow focus_window, LPARAM lParam)
{
    CenterWindow();

    return TRUE;
}

void AddUserDialog::OnClose()
{
    EndDialog(IDCANCEL);
}

LRESULT AddUserDialog::OnOkButton(WORD notify_code, WORD id, HWND hWndCtl, BOOL& handled)
{
    EndDialog(IDOK);
    return 0;
}

LRESULT AddUserDialog::OnCancelButton(WORD notify_code, WORD id, HWND hWndCtl, BOOL& handled)
{
    PostMessageW(WM_CLOSE);
    return 0;
}

} // namespace aspia
