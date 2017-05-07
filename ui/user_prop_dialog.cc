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
#include "host/host_user_utils.h"
#include "crypto/sha512.h"

#include <commctrl.h>
#include <uxtheme.h>
#include <windowsx.h>

namespace aspia {

UserPropDialog::UserPropDialog(Mode mode, proto::HostUser* user) :
    mode_(mode), user_(user)
{
    // Nothing
}

INT_PTR UserPropDialog::DoModal(HWND parent)
{
    return Run(Module::Current(), parent, IDD_USER_PROP);
}

void UserPropDialog::InsertSessionType(HWND list, proto::SessionType session_type)
{
    UINT string_id;

    switch (session_type)
    {
        case proto::SessionType::SESSION_DESKTOP_MANAGE:
            string_id = IDS_SESSION_TYPE_DESKTOP_MANAGE;
            break;

        case proto::SessionType::SESSION_DESKTOP_VIEW:
            string_id = IDS_SESSION_TYPE_DESKTOP_VIEW;
            break;

        case proto::SessionType::SESSION_POWER_MANAGE:
            string_id = IDS_SESSION_TYPE_POWER_MANAGE;
            break;

        case proto::SessionType::SESSION_FILE_TRANSFER:
            string_id = IDS_SESSION_TYPE_FILE_TRANSFER;
            break;

        case proto::SessionType::SESSION_SYSTEM_INFO:
            string_id = IDS_SESSION_TYPE_SYSTEM_INFO;
            break;

        default:
            return;
    }

    std::wstring item_text = module().string(string_id);

    LVITEMW item = { 0 };
    item.mask    = LVIF_TEXT | LVIF_PARAM;
    item.pszText = &item_text[0];
    item.iItem   = ListView_GetItemCount(list);
    item.lParam  = session_type;

    int item_index = ListView_InsertItem(list, &item);

    ListView_SetCheckState(list,
                           item_index,
                           ((user_->session_types() & session_type) ? TRUE : FALSE));
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
    HWND list = GetDlgItem(IDC_SESSION_TYPES_LIST);

    SetWindowTheme(list, L"explorer", nullptr);
    ListView_SetExtendedListViewStyle(list, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);

    RECT list_rect = { 0 };
    GetClientRect(list, &list_rect);

    LVCOLUMNW column = { 0 };
    column.mask = LVCF_FMT | LVCF_WIDTH;
    column.cx   = (list_rect.right - list_rect.left) - GetSystemMetrics(SM_CXVSCROLL);
    column.fmt  = LVCFMT_LEFT;

    ListView_InsertColumn(list, 0, &column);

    InsertSessionType(list, proto::SessionType::SESSION_DESKTOP_MANAGE);
    InsertSessionType(list, proto::SessionType::SESSION_DESKTOP_VIEW);
    InsertSessionType(list, proto::SessionType::SESSION_POWER_MANAGE);
    InsertSessionType(list, proto::SessionType::SESSION_FILE_TRANSFER);
    InsertSessionType(list, proto::SessionType::SESSION_SYSTEM_INFO);

    HWND username_edit = GetDlgItem(IDC_USERNAME_EDIT);

    if (mode_ == Mode::Edit)
    {
        password_changed_ = false;

        HWND password_edit = GetDlgItem(IDC_PASSWORD_EDIT);
        HWND password_retry_edit = GetDlgItem(IDC_PASSWORD_RETRY_EDIT);

        Edit_SetText(username_edit, UNICODEfromUTF8(user_->username()).c_str());

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

void UserPropDialog::OnClose()
{
    EndDialog(IDCANCEL);
}

void UserPropDialog::OnOkButton()
{
    std::wstring username = GetDlgItemString(IDC_USERNAME_EDIT);
    if (!IsValidUserName(username))
    {
        MessageBoxW(hwnd(),
                    module().string(IDS_INVALID_USERNAME).c_str(),
                    nullptr,
                    MB_OK | MB_ICONWARNING);
        return;
    }

    if (mode_ == Mode::Add || username != UNICODEfromUTF8(user_->username()))
    {
        if (!IsUniqueUserName(username))
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
        std::wstring password = GetDlgItemString(IDC_PASSWORD_EDIT);
        if (!IsValidPassword(password))
        {
            MessageBoxW(hwnd(),
                        module().string(IDS_INVALID_PASSWORD).c_str(),
                        nullptr,
                        MB_OK | MB_ICONWARNING);
            return;
        }

        if (password != GetDlgItemString(IDC_PASSWORD_RETRY_EDIT))
        {
            MessageBoxW(hwnd(),
                        module().string(IDS_PASSWORDS_NOT_MATCH).c_str(),
                        nullptr,
                        MB_OK | MB_ICONWARNING);
            return;
        }

        bool ret = CreateSHA512(UTF8fromUNICODE(password),
                                user_->mutable_password_hash());
        CHECK(ret);
    }

    uint32_t session_types = 0;

    HWND list = GetDlgItem(IDC_SESSION_TYPES_LIST);
    int count = ListView_GetItemCount(list);

    for (int i = 0; i < count; ++i)
    {
        if (ListView_GetCheckState(list, i))
        {
            LVITEMW item = { 0 };

            item.mask = LVIF_PARAM;
            item.iItem = i;

            if (ListView_GetItem(list, &item))
            {
                session_types |= static_cast<uint32_t>(item.lParam);
            }
        }
    }

    bool is_enabled =
        IsDlgButtonChecked(hwnd(), IDC_DISABLE_USER_CHECK) == BST_UNCHECKED;

    user_->set_enabled(is_enabled);
    user_->set_username(UTF8fromUNICODE(username));
    user_->set_session_types(session_types);

    EndDialog(IDOK);
}

void UserPropDialog::OnCancelButton()
{
    EndDialog(IDCANCEL);
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
