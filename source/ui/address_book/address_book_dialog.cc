//
// PROJECT:         Aspia
// FILE:            ui/address_book/address_book_dialog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/address_book/address_book_dialog.h"

#include <atlctrls.h>
#include <atlmisc.h>

#include "base/logging.h"
#include "ui/address_book/address_book_secure_util.h"

namespace aspia {

namespace {

constexpr int kMaxNameLength = 64;
constexpr int kMinNameLength = 1;
constexpr int kMinPasswordLength = 6;
constexpr int kMaxPasswordLength = 64;
constexpr int kMaxCommentLength = 2048;

} // namespace

AddressBookDialog::AddressBookDialog(const proto::AddressBook& address_book,
                                     const proto::ComputerGroup& root_group)
    : address_book_(address_book),
      root_group_(root_group)
{
    // Nothing
}

AddressBookDialog::~AddressBookDialog()
{
    SecureClearComputerGroup(&root_group_);
    address_book_.Clear();
}

const proto::AddressBook& AddressBookDialog::GetAddressBook() const
{
    return address_book_;
}

const proto::ComputerGroup& AddressBookDialog::GetRootComputerGroup() const
{
    return root_group_;
}

LRESULT AddressBookDialog::OnInitDialog(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    DlgResize_Init();

    CString name;
    name.LoadStringW(IDS_AB_DEFAULT_ADDRESS_BOOK_NAME);

    GetDlgItem(IDC_NAME_EDIT).SetWindowTextW(name);

    CComboBox encryption_type_combo(GetDlgItem(IDC_ENCRYPTION_TYPE_COMBO));

    auto add_encryption_type =
        [&](UINT string_id, proto::AddressBook::EncryptionType encryption_type)
    {
        CString text;
        text.LoadStringW(string_id);

        int item_index = encryption_type_combo.AddString(text);
        encryption_type_combo.SetItemData(item_index, encryption_type);
    };

    add_encryption_type(IDS_AB_ENCRYPTION_NONE,
                        proto::AddressBook::ENCRYPTION_TYPE_NONE);
    add_encryption_type(IDS_AB_ENCRYPTION_XCHACHA20_POLY1305,
                        proto::AddressBook::ENCRYPTION_TYPE_XCHACHA20_POLY1305);
    encryption_type_combo.SetCurSel(0);

    GetDlgItem(IDC_PASSWORD_EDIT).EnableWindow(FALSE);
    GetDlgItem(IDC_PASSWORD_RETRY_EDIT).EnableWindow(FALSE);

    return FALSE;
}

LRESULT AddressBookDialog::OnClose(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    EndDialog(IDCANCEL);
    return 0;
}

LRESULT AddressBookDialog::OnOkButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    EndDialog(IDOK);
    return 0;
}

LRESULT AddressBookDialog::OnCancelButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    EndDialog(IDCANCEL);
    return 0;
}

LRESULT AddressBookDialog::OnEncryptionTypeChanged(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    CComboBox combo(GetDlgItem(IDC_ENCRYPTION_TYPE_COMBO));

    proto::AddressBook::EncryptionType encryption_type =
        static_cast<proto::AddressBook::EncryptionType>(combo.GetItemData(combo.GetCurSel()));

    CEdit password_edit(GetDlgItem(IDC_PASSWORD_EDIT));
    CEdit password_repeat_edit(GetDlgItem(IDC_PASSWORD_RETRY_EDIT));

    switch (encryption_type)
    {
        case proto::AddressBook::ENCRYPTION_TYPE_NONE:
        {
            password_edit.EnableWindow(FALSE);
            password_repeat_edit.EnableWindow(FALSE);
        }
        break;

        case proto::AddressBook::ENCRYPTION_TYPE_XCHACHA20_POLY1305:
        {
            password_edit.EnableWindow(TRUE);
            password_repeat_edit.EnableWindow(TRUE);
        }
        break;

        default:
            DLOG(LS_ERROR) << "Unexpected encryption type: " << encryption_type;
            break;
    }

    return 0;
}

} // namespace aspia
