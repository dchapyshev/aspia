//
// PROJECT:         Aspia
// FILE:            ui/address_book/address_book_dialog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/address_book/address_book_dialog.h"

#include <atlctrls.h>
#include <atlmisc.h>

#include "base/strings/unicode.h"
#include "base/logging.h"
#include "crypto/sha.h"
#include "ui/address_book/address_book_secure_util.h"

namespace aspia {

namespace {

constexpr int kMaxNameLength = 64;
constexpr int kMinNameLength = 1;
constexpr int kMinPasswordLength = 6;
constexpr int kMaxPasswordLength = 64;
constexpr int kMaxCommentLength = 2048;

constexpr WCHAR kStarString[] = L"******";

} // namespace

AddressBookDialog::AddressBookDialog(proto::AddressBook::EncryptionType encryption_type,
                                     const proto::ComputerGroup& root_group,
                                     const std::string& key)
    : encryption_type_(encryption_type),
      root_group_(root_group),
      password_changed_(false)
{
    key_.reset(key);
}

AddressBookDialog::~AddressBookDialog()
{
    SecureClearComputerGroup(&root_group_);
}

proto::AddressBook::EncryptionType AddressBookDialog::GetEncryptionType() const
{
    return encryption_type_;
}

const proto::ComputerGroup& AddressBookDialog::GetRootComputerGroup() const
{
    return root_group_;
}

const std::string& AddressBookDialog::GetKey() const
{
    return key_.string();
}

LRESULT AddressBookDialog::OnInitDialog(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    DlgResize_Init();

    if (root_group_.name().empty())
    {
        CString name;
        name.LoadStringW(IDS_AB_DEFAULT_ADDRESS_BOOK_NAME);
        SetDlgItemTextW(IDC_NAME_EDIT, name);
    }
    else
    {
        SetDlgItemTextW(IDC_NAME_EDIT, UNICODEfromUTF8(root_group_.name()).c_str());
    }

    if (!root_group_.comment().empty())
    {
        SetDlgItemTextW(IDC_COMMENT_EDIT, UNICODEfromUTF8(root_group_.comment()).c_str());
    }

    CComboBox encryption_type_combo(GetDlgItem(IDC_ENCRYPTION_TYPE_COMBO));

    auto add_encryption_type =
        [&](UINT string_id, proto::AddressBook::EncryptionType encryption_type)
    {
        CString text;
        text.LoadStringW(string_id);

        int item_index = encryption_type_combo.AddString(text);
        encryption_type_combo.SetItemData(item_index, encryption_type);

        if (encryption_type == encryption_type_)
            encryption_type_combo.SetCurSel(item_index);
    };

    add_encryption_type(IDS_AB_ENCRYPTION_NONE,
                        proto::AddressBook::ENCRYPTION_TYPE_NONE);
    add_encryption_type(IDS_AB_ENCRYPTION_XCHACHA20_POLY1305,
                        proto::AddressBook::ENCRYPTION_TYPE_XCHACHA20_POLY1305);

    // If there is no selected item.
    if (encryption_type_combo.GetCurSel() == CB_ERR)
    {
        // Select first item.
        encryption_type_combo.SetCurSel(0);
    }

    CEdit password_edit(GetDlgItem(IDC_PASSWORD_EDIT));
    CEdit password_repeat_edit(GetDlgItem(IDC_PASSWORD_RETRY_EDIT));

    if (encryption_type_ == proto::AddressBook::ENCRYPTION_TYPE_XCHACHA20_POLY1305)
    {
        DCHECK(!key_.string().empty());

        password_edit.SetWindowTextW(kStarString);
        password_edit.SetReadOnly(TRUE);
        password_repeat_edit.SetWindowTextW(kStarString);
        password_repeat_edit.SetReadOnly(TRUE);

        // Setup our message handler.
        SetWindowSubclass(password_edit,
                          PasswordEditWindowProc,
                          0,
                          reinterpret_cast<DWORD_PTR>(this));

        SetWindowSubclass(password_repeat_edit,
                          PasswordEditWindowProc,
                          0,
                          reinterpret_cast<DWORD_PTR>(this));
    }
    else
    {
        DCHECK_EQ(encryption_type_, proto::AddressBook::ENCRYPTION_TYPE_NONE);

        password_edit.EnableWindow(FALSE);
        password_repeat_edit.EnableWindow(FALSE);
    }

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
    CEdit name_edit(GetDlgItem(IDC_NAME_EDIT));

    int name_length = name_edit.GetWindowTextLengthW();
    if (name_length > kMaxNameLength)
    {
        ShowErrorMessage(IDS_AB_TOO_LONG_NAME_ERROR);
        return 0;
    }
    else if (name_length < kMinNameLength)
    {
        ShowErrorMessage(IDS_AB_NAME_CANT_BE_EMPTY_ERROR);
        return 0;
    }
    else
    {
        WCHAR name[kMaxNameLength + 1];
        name_edit.GetWindowTextW(name, _countof(name));
        root_group_.set_name(UTF8fromUNICODE(name));
    }

    CEdit comment_edit(GetDlgItem(IDC_COMMENT_EDIT));
    if (comment_edit.GetWindowTextLengthW() > kMaxCommentLength)
    {
        ShowErrorMessage(IDS_AB_TOO_LONG_COMMENT_ERROR);
        return 0;
    }
    else
    {
        WCHAR comment[kMaxCommentLength + 1];
        comment_edit.GetWindowTextW(comment, _countof(comment));
        root_group_.set_comment(UTF8fromUNICODE(comment));
    }

    CComboBox encryption_type_combo(GetDlgItem(IDC_ENCRYPTION_TYPE_COMBO));

    encryption_type_ = static_cast<proto::AddressBook::EncryptionType>(
        encryption_type_combo.GetItemData(encryption_type_combo.GetCurSel()));

    switch (encryption_type_)
    {
        case proto::AddressBook::ENCRYPTION_TYPE_NONE:
        {
            key_.reset();
        }
        break;

        case proto::AddressBook::ENCRYPTION_TYPE_XCHACHA20_POLY1305:
        {
            if (password_changed_)
            {
                CEdit password_edit(GetDlgItem(IDC_PASSWORD_EDIT));
                CEdit password_repeat_edit(GetDlgItem(IDC_PASSWORD_RETRY_EDIT));

                SecureArray<WCHAR, 256> password;

                password_edit.GetWindowTextW(password.get(), static_cast<int>(password.count()));
                if (wcslen(password.get()) < kMinPasswordLength)
                {
                    ShowErrorMessage(IDS_INVALID_PASSWORD);
                    return 0;
                }

                SecureArray<WCHAR, 256> password_repeat;

                password_repeat_edit.GetWindowTextW(password_repeat.get(),
                                                    static_cast<int>(password_repeat.count()));
                if (wcscmp(password_repeat.get(), password.get()) != 0)
                {
                    ShowErrorMessage(IDS_PASSWORDS_NOT_MATCH);
                    return 0;
                }

                key_.reset();

                SecureString<std::string> password_utf8(UTF8fromUNICODE(password.get()));
                SHA256(password_utf8.string(), key_.mutable_string(), 1000);
            }
        }
        break;

        default:
            DLOG(LS_FATAL) << "Unexpected encryption type: " << encryption_type_;
            break;
    }

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

void AddressBookDialog::ShowErrorMessage(UINT string_id)
{
    CString message;
    message.LoadStringW(string_id);
    MessageBoxW(message, nullptr, MB_OK | MB_ICONWARNING);
}

void AddressBookDialog::OnPasswordEditDblClick()
{
    CEdit password_edit(GetDlgItem(IDC_PASSWORD_EDIT));
    CEdit password_retry_edit(GetDlgItem(IDC_PASSWORD_RETRY_EDIT));

    password_edit.SetReadOnly(FALSE);
    password_retry_edit.SetReadOnly(FALSE);
    password_edit.SetWindowTextW(L"");
    password_retry_edit.SetWindowTextW(L"");

    password_changed_ = true;
}

//static
LRESULT CALLBACK AddressBookDialog::PasswordEditWindowProc(HWND hwnd,
                                                           UINT msg,
                                                           WPARAM wparam,
                                                           LPARAM lparam,
                                                           UINT_PTR subclass_id,
                                                           DWORD_PTR ref_data)
{
    switch (msg)
    {
        case WM_LBUTTONDBLCLK:
        {
            AddressBookDialog* self = reinterpret_cast<AddressBookDialog*>(ref_data);
            self->OnPasswordEditDblClick();
        }
        break;

        case WM_NCDESTROY:
        {
            RemoveWindowSubclass(hwnd, PasswordEditWindowProc, subclass_id);
        }
        break;

        default:
            break;
    }

    // Get the default message handler for edit control.
    WNDPROC window_proc = reinterpret_cast<WNDPROC>(GetClassLongPtrW(hwnd, GCLP_WNDPROC));

    return window_proc(hwnd, msg, wparam, lparam);
}
} // namespace aspia
