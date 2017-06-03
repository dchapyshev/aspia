//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/user_prop_dialog.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/user_prop_dialog.h"
#include "ui/resource.h"
#include "base/unicode.h"
#include "base/logging.h"
#include "host/host_user_list.h"
#include "crypto/sha512.h"
#include "crypto/secure_string.h"

#include <windowsx.h>

namespace aspia {

UserPropDialog::UserPropDialog(Mode mode,
                               proto::HostUser* user,
                               const HostUserList& user_list) :
    mode_(mode),
    user_(user),
    user_list_(user_list)
{
    DCHECK(user_);
}

INT_PTR UserPropDialog::DoModal(HWND parent)
{
    return Run(Module::Current(), parent, IDD_USER_PROP);
}

void UserPropDialog::InsertSessionType(ListView& list,
                                       proto::SessionType session_type,
                                       UINT string_id)
{
    int item_index = list.AddItem(module().string(string_id), session_type);

    list.SetCheckState(item_index,
                       (user_->session_types() & session_type) ? true : false);
}

void UserPropDialog::OnPasswordEditDblClick()
{
    HWND password_edit = GetDlgItem(IDC_PASSWORD_EDIT);
    HWND password_retry_edit = GetDlgItem(IDC_PASSWORD_RETRY_EDIT);

    Edit_SetReadOnly(password_edit, FALSE);
    Edit_SetReadOnly(password_retry_edit, FALSE);
    Edit_SetText(password_edit, L"");
    Edit_SetText(password_retry_edit, L"");

    password_changed_ = true;
}

//static
LRESULT CALLBACK UserPropDialog::PasswordEditWindowProc(HWND hwnd,
                                                        UINT msg,
                                                        WPARAM wparam,
                                                        LPARAM lparam,
                                                        UINT_PTR subclass_id,
                                                        DWORD_PTR ref_data)
{
    if (msg == WM_LBUTTONDBLCLK)
    {
        UserPropDialog* self = reinterpret_cast<UserPropDialog*>(ref_data);
        self->OnPasswordEditDblClick();
    }

    // Get the default message handler for edit control.
    WNDPROC window_proc =
        reinterpret_cast<WNDPROC>(GetClassLongPtrW(hwnd, GCLP_WNDPROC));

    return window_proc(hwnd, msg, wparam, lparam);
}

void UserPropDialog::OnInitDialog()
{
    ListView list(GetDlgItem(IDC_SESSION_TYPES_LIST));

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

    HWND username_edit = GetDlgItem(IDC_USERNAME_EDIT);

    if (mode_ == Mode::Edit)
    {
        password_changed_ = false;

        HWND password_edit = GetDlgItem(IDC_PASSWORD_EDIT);
        HWND password_retry_edit = GetDlgItem(IDC_PASSWORD_RETRY_EDIT);

        SecureString<std::wstring> username;
        CHECK(UTF8toUNICODE(user_->username(), username));
        Edit_SetText(username_edit, username.c_str());

        const WCHAR kNotChangedPassword[] = L"******";
        Edit_SetText(password_edit, kNotChangedPassword);
        Edit_SetText(password_retry_edit, kNotChangedPassword);

        // Setup our message handler.
        SetWindowSubclass(password_edit,
                          PasswordEditWindowProc,
                          0,
                          reinterpret_cast<DWORD_PTR>(this));

        SetWindowSubclass(password_retry_edit,
                          PasswordEditWindowProc,
                          0,
                          reinterpret_cast<DWORD_PTR>(this));

        Edit_SetReadOnly(password_edit, TRUE);
        Edit_SetReadOnly(password_retry_edit, TRUE);

        CheckDlgButton(hwnd(),
                       IDC_DISABLE_USER_CHECK,
                       user_->enabled() ? BST_UNCHECKED : BST_CHECKED);
    }

    SetFocus(username_edit);
}

void UserPropDialog::OnOkButton()
{
    SecureString<std::wstring> username(GetDlgItemString(IDC_USERNAME_EDIT));

    if (!HostUserList::IsValidUserName(username))
    {
        MessageBoxW(hwnd(),
                    module().string(IDS_INVALID_USERNAME).c_str(),
                    nullptr,
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
            MessageBoxW(hwnd(),
                        module().string(IDS_USER_ALREADY_EXISTS).c_str(),
                        nullptr,
                        MB_OK | MB_ICONWARNING);
            return;
        }
    }

    if (mode_ == Mode::Add || password_changed_)
    {
        SecureString<std::wstring> password(GetDlgItemString(IDC_PASSWORD_EDIT));
        if (!HostUserList::IsValidPassword(password))
        {
            MessageBoxW(hwnd(),
                        module().string(IDS_INVALID_PASSWORD).c_str(),
                        nullptr,
                        MB_OK | MB_ICONWARNING);
            return;
        }

        SecureString<std::wstring> password_retry =
            GetDlgItemString(IDC_PASSWORD_RETRY_EDIT);

        if (password != password_retry)
        {
            MessageBoxW(hwnd(),
                        module().string(IDS_PASSWORDS_NOT_MATCH).c_str(),
                        nullptr,
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

    ListView list(GetDlgItem(IDC_SESSION_TYPES_LIST));
    int count = list.GetItemCount();

    for (int i = 0; i < count; ++i)
    {
        if (list.GetCheckState(i))
            session_types |= list.GetItemData<uint32_t>(i);
    }

    bool is_enabled =
        IsDlgButtonChecked(hwnd(), IDC_DISABLE_USER_CHECK) == BST_UNCHECKED;

    CHECK(UNICODEtoUTF8(username, *user_->mutable_username()));

    user_->set_enabled(is_enabled);
    user_->set_session_types(session_types);

    EndDialog(IDOK);
}

INT_PTR UserPropDialog::OnMessage(UINT msg, WPARAM wparam, LPARAM lparam)
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
