//
// PROJECT:         Aspia Remote Desktop
// FILE:            gui/users_dialog.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "gui/users_dialog.h"
#include "gui/add_user_dialog.h"
#include "base/process.h"

namespace aspia {

BOOL UsersDialog::OnInitDialog(CWindow focus_window, LPARAM lParam)
{
    CenterWindow();

    if (!Process::Current().HasAdminRights())
    {
        GetDlgItem(IDC_USER_LIST).EnableWindow(FALSE);
        GetDlgItem(IDÑ_ADD_USER).EnableWindow(FALSE);
        GetDlgItem(IDC_EDIT_USER).EnableWindow(FALSE);
        GetDlgItem(IDC_DELETE_USER).EnableWindow(FALSE);
    }

    return TRUE;
}

void UsersDialog::OnClose()
{
    EndDialog(IDCANCEL);
}

LRESULT UsersDialog::OnAddButton(WORD notify_code, WORD id, HWND hWndCtl, BOOL& handled)
{
    AddUserDialog dialog;
    dialog.DoModal();
    return 0;
}

LRESULT UsersDialog::OnEditButton(WORD notify_code, WORD id, HWND hWndCtl, BOOL& handled)
{
    return 0;
}

LRESULT UsersDialog::OnDeleteButton(WORD notify_code, WORD id, HWND hWndCtl, BOOL& handled)
{
    return 0;
}

LRESULT UsersDialog::OnOkButton(WORD notify_code, WORD id, HWND hWndCtl, BOOL& handled)
{
    EndDialog(IDOK);
    return 0;
}

LRESULT UsersDialog::OnCancelButton(WORD notify_code, WORD id, HWND hWndCtl, BOOL& handled)
{
    PostMessageW(WM_CLOSE);
    return 0;
}

} // namespace aspia
