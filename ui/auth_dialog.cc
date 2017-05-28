//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/auth_dialog.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/auth_dialog.h"
#include "ui/resource.h"
#include "base/unicode.h"
#include "base/logging.h"

namespace aspia {

AuthDialog::~AuthDialog()
{
    SecureZeroMemory(&username_[0], username_.size());
    SecureZeroMemory(&password_[0], password_.size());
}

INT_PTR AuthDialog::DoModal(HWND parent)
{
    return Run(Module().Current(), parent, IDD_AUTH);
}

void AuthDialog::OnInitDialog()
{
    SetForegroundWindowEx();
    CenterWindow();
}

void AuthDialog::OnClose()
{
    EndDialog(IDCANCEL);
}

void AuthDialog::OnOkButton()
{
    WCHAR buffer[128];

    if (GetDlgItemTextW(hwnd(), IDC_USERNAME_EDIT, buffer, _countof(buffer)))
    {
        CHECK(UNICODEtoUTF8(buffer, username_));
        SecureZeroMemory(buffer, sizeof(buffer));
    }

    if (GetDlgItemTextW(hwnd(), IDC_PASSWORD_EDIT, buffer, _countof(buffer)))
    {
        CHECK(UNICODEtoUTF8(buffer, password_));
        SecureZeroMemory(buffer, sizeof(buffer));
    }

    EndDialog(IDOK);
}

void AuthDialog::OnCancelButton()
{
    PostMessageW(hwnd(), WM_CLOSE, 0, 0);
}

std::string AuthDialog::UserName()
{
    return username_;
}

std::string AuthDialog::Password()
{
    return password_;
}

INT_PTR AuthDialog::OnMessage(UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
        case WM_INITDIALOG:
            OnInitDialog();
            break;

        case WM_COMMAND:
        {
            switch (LOWORD(wparam))
            {
                case IDOK:
                    OnOkButton();
                    break;

                case IDCANCEL:
                    OnCancelButton();
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

} // namespace aspia
