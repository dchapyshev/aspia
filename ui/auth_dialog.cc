//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/auth_dialog.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/auth_dialog.h"
#include "ui/resource.h"
#include "base/strings/unicode.h"
#include "base/logging.h"

namespace aspia {

INT_PTR UiAuthDialog::DoModal(HWND parent)
{
    return Run(UiModule().Current(), parent, IDD_AUTH);
}

void UiAuthDialog::OnInitDialog()
{
    SetForegroundWindowEx();
    CenterWindow();
}

void UiAuthDialog::OnClose()
{
    EndDialog(IDCANCEL);
}

void UiAuthDialog::OnOkButton()
{
    SecureString<std::wstring> username(GetDlgItemString(IDC_USERNAME_EDIT));

    if (!username.empty())
        UNICODEtoUTF8(username, username_);

    SecureString<std::wstring> password(GetDlgItemString(IDC_PASSWORD_EDIT));

    if (!password.empty())
        UNICODEtoUTF8(password, password_);

    EndDialog(IDOK);
}

void UiAuthDialog::OnCancelButton()
{
    PostMessageW(WM_CLOSE, 0, 0);
}

const std::string& UiAuthDialog::UserName() const
{
    return username_;
}

const std::string& UiAuthDialog::Password() const
{
    return password_;
}

INT_PTR UiAuthDialog::OnMessage(UINT msg, WPARAM wparam, LPARAM lparam)
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
