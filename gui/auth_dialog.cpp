//
// PROJECT:         Aspia Remote Desktop
// FILE:            gui/auth_dialog.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "gui/auth_dialog.h"

#include "base/unicode.h"

namespace aspia {

BOOL AuthDialog::OnInitDialog(CWindow focus_window, LPARAM lParam)
{
    CenterWindow();
    return TRUE;
}

void AuthDialog::OnClose()
{
    EndDialog(IDCANCEL);
}

LRESULT AuthDialog::OnOkButton(WORD notify_code, WORD id, HWND hWndCtl, BOOL& handled)
{
    WCHAR buffer[128] = { 0 };

    if (GetDlgItemTextW(IDC_USERNAME_EDIT, buffer, ARRAYSIZE(buffer)))
        username_ = UTF8fromUNICODE(buffer);

    SecureZeroMemory(buffer, sizeof(buffer));

    if (GetDlgItemTextW(IDC_PASSWORD_EDIT, buffer, ARRAYSIZE(buffer)))
        password_ = UTF8fromUNICODE(buffer);

    SecureZeroMemory(buffer, sizeof(buffer));

    EndDialog(IDOK);
    return 0;
}

LRESULT AuthDialog::OnCancelButton(WORD notify_code, WORD id, HWND hWndCtl, BOOL& handled)
{
    PostMessageW(WM_CLOSE);
    return 0;
}

std::string AuthDialog::UserName()
{
    return username_;
}

std::string AuthDialog::Password()
{
    return password_;
}

} // namespace aspia
