//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/user_prop_dialog.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/user_prop_dialog.h"
#include "ui/base/edit.h"
#include "ui/resource.h"
#include "base/strings/unicode.h"
#include "base/logging.h"
#include "host/host_user_list.h"
#include "crypto/sha512.h"
#include "crypto/secure_string.h"

#include <windowsx.h>

namespace aspia {

UiUserPropDialog::UiUserPropDialog(Mode mode,
                                   proto::HostUser* user,
                                   const HostUserList& user_list) :
    mode_(mode),
    user_(user),
    user_list_(user_list)
{
    DCHECK(user_);
}

INT_PTR UiUserPropDialog::DoModal(HWND parent)
{
    return Run(UiModule::Current(), parent, IDD_USER_PROP);
}

void UiUserPropDialog::InsertSessionType(UiListView& list,
                                         proto::SessionType session_type,
                                         UINT string_id)
{
    int item_index = list.AddItem(Module().String(string_id), session_type);

    list.SetCheckState(item_index,
                       (user_->session_types() & session_type) ? true : false);
}

void UiUserPropDialog::OnPasswordEditDblClick()
{
    UiEdit password_edit(GetDlgItem(IDC_PASSWORD_EDIT));
    UiEdit password_retry_edit(GetDlgItem(IDC_PASSWORD_RETRY_EDIT));

    password_edit.SetReadOnly(false);
    password_retry_edit.SetReadOnly(false);
    password_edit.SetWindowString(L"");
    password_retry_edit.SetWindowString(L"");

    password_changed_ = true;
}

//static
LRESULT CALLBACK UiUserPropDialog::PasswordEditWindowProc(HWND hwnd,
                                                          UINT msg,
                                                          WPARAM wparam,
                                                          LPARAM lparam,
                                                          UINT_PTR subclass_id,
                                                          DWORD_PTR ref_data)
{
    if (msg == WM_LBUTTONDBLCLK)
    {
        UiUserPropDialog* self = reinterpret_cast<UiUserPropDialog*>(ref_data);
        self->OnPasswordEditDblClick();
    }

    // Get the default message handler for edit control.
    WNDPROC window_proc =
        reinterpret_cast<WNDPROC>(GetClassLongPtrW(hwnd, GCLP_WNDPROC));

    return window_proc(hwnd, msg, wparam, lparam);
}

void UiUserPropDialog::OnInitDialog()
{
    UiListView list(GetDlgItem(IDC_SESSION_TYPES_LIST));

    list.ModifyExtendedListViewStyle(0, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
    list.AddOnlyOneColumn();

    InsertSessionType(list,
                      proto::SessionType::SESSION_TYPE_DESKTOP_MANAGE,
                      IDS_SESSION_TYPE_DESKTOP_MANAGE);

    InsertSessionType(list,
                      proto::SessionType::SESSION_TYPE_DESKTOP_VIEW,
                      IDS_SESSION_TYPE_DESKTOP_VIEW);

    InsertSessionType(list,
                      proto::SessionType::SESSION_TYPE_POWER_MANAGE,
                      IDS_SESSION_TYPE_POWER_MANAGE);

    InsertSessionType(list,
                      proto::SessionType::SESSION_TYPE_FILE_TRANSFER,
                      IDS_SESSION_TYPE_FILE_TRANSFER);

    InsertSessionType(list,
                      proto::SessionType::SESSION_TYPE_SYSTEM_INFO,
                      IDS_SESSION_TYPE_SYSTEM_INFO);

    UiEdit username_edit(GetDlgItem(IDC_USERNAME_EDIT));

    if (mode_ == Mode::Edit)
    {
        password_changed_ = false;

        UiEdit password_edit(GetDlgItem(IDC_PASSWORD_EDIT));
        UiEdit password_retry_edit(GetDlgItem(IDC_PASSWORD_RETRY_EDIT));

        SecureString<std::wstring> username;
        CHECK(UTF8toUNICODE(user_->username(), username));
        username_edit.SetWindowString(username);

        const WCHAR kNotChangedPassword[] = L"******";
        password_edit.SetWindowString(kNotChangedPassword);
        password_retry_edit.SetWindowString(kNotChangedPassword);

        // Setup our message handler.
        SetWindowSubclass(password_edit,
                          PasswordEditWindowProc,
                          0,
                          reinterpret_cast<DWORD_PTR>(this));

        SetWindowSubclass(password_retry_edit,
                          PasswordEditWindowProc,
                          0,
                          reinterpret_cast<DWORD_PTR>(this));

        password_edit.SetReadOnly(true);
        password_retry_edit.SetReadOnly(true);

        CheckDlgButton(IDC_DISABLE_USER_CHECK,
                       user_->enabled() ? BST_UNCHECKED : BST_CHECKED);
    }

    SetFocus(username_edit);
}

void UiUserPropDialog::OnOkButton()
{
    SecureString<std::wstring> username(GetDlgItemString(IDC_USERNAME_EDIT));

    if (!HostUserList::IsValidUserName(username))
    {
        MessageBoxW(Module().String(IDS_INVALID_USERNAME),
                    MB_OK | MB_ICONWARNING);
        return;
    }

    SecureString<std::wstring> prev_username;

    if (!user_->username().empty())
    {
        CHECK(UTF8toUNICODE(user_->username(), prev_username));
    }

    if (username != prev_username)
    {
        if (!user_list_.IsUniqueUserName(username))
        {
            MessageBoxW(Module().String(IDS_USER_ALREADY_EXISTS),
                        MB_OK | MB_ICONWARNING);
            return;
        }
    }

    if (mode_ == Mode::Add || password_changed_)
    {
        SecureString<std::wstring> password(GetDlgItemString(IDC_PASSWORD_EDIT));
        if (!HostUserList::IsValidPassword(password))
        {
            MessageBoxW(Module().String(IDS_INVALID_PASSWORD),
                        MB_OK | MB_ICONWARNING);
            return;
        }

        SecureString<std::wstring> password_retry =
            GetDlgItemString(IDC_PASSWORD_RETRY_EDIT);

        if (password != password_retry)
        {
            MessageBoxW(Module().String(IDS_PASSWORDS_NOT_MATCH),
                        MB_OK | MB_ICONWARNING);
            return;
        }

        SecureString<std::string> password_in_utf8;
        CHECK(UNICODEtoUTF8(password, password_in_utf8));

        bool ret = HostUserList::CreatePasswordHash(password_in_utf8,
                                                    *user_->mutable_password_hash());
        DCHECK(ret);
    }

    uint32_t session_types = 0;

    UiListView list(GetDlgItem(IDC_SESSION_TYPES_LIST));
    int count = list.GetItemCount();

    for (int i = 0; i < count; ++i)
    {
        if (list.GetCheckState(i))
            session_types |= list.GetItemData<uint32_t>(i);
    }

    bool is_enabled =
        IsDlgButtonChecked(IDC_DISABLE_USER_CHECK) == BST_UNCHECKED;

    CHECK(UNICODEtoUTF8(username, *user_->mutable_username()));

    user_->set_enabled(is_enabled);
    user_->set_session_types(session_types);

    EndDialog(IDOK);
}

INT_PTR UiUserPropDialog::OnMessage(UINT msg, WPARAM wparam, LPARAM lparam)
{
    UNREF(lparam);

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
                    EndDialog(IDCANCEL);
                    break;
            }
        }
        break;

        case WM_CLOSE:
            EndDialog(IDCANCEL);
            break;
    }

    return 0;
}

} // namespace aspia
