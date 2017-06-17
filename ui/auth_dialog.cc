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

extern "C" {
#define SODIUM_STATIC
#include <sodium.h>
} // extern "C"

namespace aspia {

UiAuthDialog::~UiAuthDialog()
{
    sodium_memzero(&username_[0], username_.size());
    sodium_memzero(&password_[0], password_.size());
}

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
    std::wstring username(GetDlgItemString(IDC_USERNAME_EDIT));

    if (!username.empty())
    {
        UNICODEtoUTF8(username, username_);
        sodium_memzero(&username[0], username.size());
    }

    std::wstring password(GetDlgItemString(IDC_PASSWORD_EDIT));

    if (!password.empty())
    {
        UNICODEtoUTF8(password, password_);
        sodium_memzero(&password[0], password.size());
    }

    EndDialog(IDOK);
}

void UiAuthDialog::OnCancelButton()
{
    PostMessageW(hwnd(), WM_CLOSE, 0, 0);
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
