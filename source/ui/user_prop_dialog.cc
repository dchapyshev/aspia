//
// PROJECT:         Aspia
// FILE:            ui/user_prop_dialog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/user_prop_dialog.h"

#include <atlmisc.h>
#include <uxtheme.h>

#include "base/strings/string_util.h"
#include "base/strings/unicode.h"
#include "base/version_helpers.h"
#include "base/logging.h"
#include "host/host_user_list.h"
#include "crypto/secure_memory.h"
#include "protocol/authorization.h"

namespace aspia {

UserPropDialog::UserPropDialog(Mode mode, proto::HostUser* user, const HostUserList& user_list)
    : mode_(mode),
      user_(user),
      user_list_(user_list)
{
    DCHECK(user_);
}

void UserPropDialog::InsertSessionType(CListViewCtrl& list,
                                       proto::auth::SessionType session_type,
                                       UINT string_id)
{
    CString text;
    text.LoadStringW(string_id);

    int item_index = list.AddItem(list.GetItemCount(), 0, text);

    list.SetItemData(item_index, session_type);
    list.SetCheckState(item_index,
                       (user_->session_types() & session_type));
}

void UserPropDialog::OnPasswordEditDblClick()
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
LRESULT CALLBACK UserPropDialog::PasswordEditWindowProc(HWND hwnd,
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
            UserPropDialog* self = reinterpret_cast<UserPropDialog*>(ref_data);
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

LRESULT UserPropDialog::OnInitDialog(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    DlgResize_Init();

    CListViewCtrl list(GetDlgItem(IDC_SESSION_TYPES_LIST));

    DWORD ex_style = LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT;

    if (IsWindowsVistaOrGreater())
    {
        SetWindowTheme(list, L"explorer", nullptr);
        ex_style |= LVS_EX_DOUBLEBUFFER;
    }

    list.SetExtendedListViewStyle(ex_style);
    list.AddColumn(L"", 0);

    InsertSessionType(list,
                      proto::auth::SESSION_TYPE_DESKTOP_MANAGE,
                      IDS_SESSION_TYPE_DESKTOP_MANAGE);

    InsertSessionType(list,
                      proto::auth::SESSION_TYPE_DESKTOP_VIEW,
                      IDS_SESSION_TYPE_DESKTOP_VIEW);

    InsertSessionType(list,
                      proto::auth::SESSION_TYPE_POWER_MANAGE,
                      IDS_SESSION_TYPE_POWER_MANAGE);

    InsertSessionType(list,
                      proto::auth::SESSION_TYPE_FILE_TRANSFER,
                      IDS_SESSION_TYPE_FILE_TRANSFER);

    InsertSessionType(list,
                      proto::auth::SESSION_TYPE_SYSTEM_INFO,
                      IDS_SESSION_TYPE_SYSTEM_INFO);

    CEdit username_edit(GetDlgItem(IDC_USERNAME_EDIT));

    if (mode_ == Mode::EDIT)
    {
        username_edit.SetWindowTextW(UNICODEfromUTF8(user_->username()).c_str());

        password_changed_ = false;

        CEdit password_edit(GetDlgItem(IDC_PASSWORD_EDIT));
        CEdit password_retry_edit(GetDlgItem(IDC_PASSWORD_RETRY_EDIT));

        const WCHAR kNotChangedPassword[] = L"******";
        password_edit.SetWindowTextW(kNotChangedPassword);
        password_retry_edit.SetWindowTextW(kNotChangedPassword);

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

    username_edit.SetFocus();
    return TRUE;
}

LRESULT UserPropDialog::OnClose(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    EndDialog(IDCANCEL);
    return 0;
}

LRESULT UserPropDialog::OnSize(UINT message, WPARAM wparam, LPARAM lparam, BOOL& /* handled */)
{
    LRESULT ret = 0;

    if (CDialogResize<UserPropDialog>::ProcessWindowMessage(
        *this, message, wparam, lparam, ret))
    {
        CListViewCtrl list(GetDlgItem(IDC_SESSION_TYPES_LIST));

        CRect list_rect;
        list.GetClientRect(list_rect);
        list.SetColumnWidth(0, list_rect.Width() - GetSystemMetrics(SM_CXVSCROLL));
    }

    return ret;
}

void UserPropDialog::ShowErrorMessage(UINT string_id)
{
    CString message;
    message.LoadStringW(string_id);
    MessageBoxW(message, nullptr, MB_OK | MB_ICONWARNING);
}

LRESULT UserPropDialog::OnOkButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    WCHAR username[128] = { 0 };
    GetDlgItemTextW(IDC_USERNAME_EDIT, username, _countof(username));

    if (!IsValidUserName(username))
    {
        ShowErrorMessage(IDS_INVALID_USERNAME);
        return 0;
    }

    std::wstring prev_username;

    if (!user_->username().empty())
        prev_username = UNICODEfromUTF8(user_->username());

    if (CompareCaseInsensitive(username, prev_username) != 0)
    {
        if (!user_list_.IsUniqueUserName(username))
        {
            ShowErrorMessage(IDS_USER_ALREADY_EXISTS);
            return 0;
        }
    }

    if (mode_ == Mode::ADD || password_changed_)
    {
        SecureArray<WCHAR, 128> password;
        GetDlgItemTextW(IDC_PASSWORD_EDIT, password.get(), static_cast<int>(password.count()));

        if (!IsValidPassword(password.get()))
        {
            ShowErrorMessage(IDS_INVALID_PASSWORD);
            return 0;
        }

        SecureArray<WCHAR, 128> password_repeat;
        GetDlgItemTextW(IDC_PASSWORD_RETRY_EDIT,
                        password_repeat.get(),
                        static_cast<int>(password_repeat.count()));

        if (wcscmp(password.get(), password_repeat.get()) == 0)
        {
            ShowErrorMessage(IDS_PASSWORDS_NOT_MATCH);
            return 0;
        }

        user_->set_password_hash(CreatePasswordHash(password.get()));
    }

    uint32_t session_types = 0;

    CListViewCtrl list(GetDlgItem(IDC_SESSION_TYPES_LIST));
    int count = list.GetItemCount();

    for (int i = 0; i < count; ++i)
    {
        if (list.GetCheckState(i))
            session_types |= static_cast<uint32_t>(list.GetItemData(i));
    }

    bool is_enabled = IsDlgButtonChecked(IDC_DISABLE_USER_CHECK) == BST_UNCHECKED;

    user_->set_username(UTF8fromUNICODE(username));
    user_->set_enabled(is_enabled);
    user_->set_session_types(session_types);

    EndDialog(IDOK);
    return 0;
}

LRESULT UserPropDialog::OnCancelButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    EndDialog(IDCANCEL);
    return 0;
}

} // namespace aspia
