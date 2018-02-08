//
// PROJECT:         Aspia
// FILE:            ui/address_book/open_address_book_dialog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/address_book/open_address_book_dialog.h"

#include "base/strings/unicode.h"
#include "crypto/secure_memory.h"
#include "ui/ui_util.h"

namespace aspia {

const std::wstring& OpenAddressBookDialog::GetPassword() const
{
    return password_.string();
}

LRESULT OpenAddressBookDialog::OnInitDialog(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    GetDlgItem(IDC_PASSWORD_EDIT).SetFocus();
    return TRUE;
}

LRESULT OpenAddressBookDialog::OnClose(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    EndDialog(IDCANCEL);
    return 0;
}

LRESULT OpenAddressBookDialog::OnOkButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    password_ = GetWindowString(GetDlgItem(IDC_PASSWORD_EDIT));

    EndDialog(IDOK);
    return 0;
}

LRESULT OpenAddressBookDialog::OnCancelButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    EndDialog(IDCANCEL);
    return 0;
}

} // namespace aspia
