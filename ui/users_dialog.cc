//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/users_dialog.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/users_dialog.h"
#include "ui/user_prop_dialog.h"
#include "base/process/process_helpers.h"
#include "base/strings/string_util.h"
#include "base/strings/unicode.h"
#include "base/version_helpers.h"
#include "base/logging.h"
#include "crypto/secure_string.h"

#include <uxtheme.h>

namespace aspia {

void UiUsersDialog::UpdateUserList()
{
    CListViewCtrl list(GetDlgItem(IDC_USER_LIST));

    list.DeleteAllItems();

    const int size = user_list_.size();

    for (int i = 0; i < size; ++i)
    {
        const proto::HostUser& user = user_list_.host_user(i);

        SecureString<std::wstring> username;
        CHECK(UTF8toUNICODE(user.username(), username));

        int item_index = list.AddItem(list.GetItemCount(),
                                      0,
                                      username.c_str(),
                                      user.enabled() ? 0 : 1);

        list.SetItemData(item_index, i);
    }
}

static int GetICLColor()
{
    DEVMODEW mode = { 0 };
    mode.dmSize = sizeof(mode);

    if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &mode))
    {
        switch (mode.dmBitsPerPel)
        {
            case 32:
                return ILC_COLOR32;

            case 24:
                return ILC_COLOR24;

            case 16:
                return ILC_COLOR16;

            case 8:
                return ILC_COLOR8;

            case 4:
                return ILC_COLOR4;
        }
    }

    return ILC_COLOR32;
}

LRESULT UiUsersDialog::OnInitDialog(UINT message,
                                    WPARAM wparam,
                                    LPARAM lparam,
                                    BOOL& handled)
{
    if (imagelist_.Create(GetSystemMetrics(SM_CXSMICON),
                          GetSystemMetrics(SM_CYSMICON),
                          ILC_MASK | GetICLColor(),
                          1, 1))
    {
        CIcon icon;

        icon = AtlLoadIconImage(IDI_USER,
                                LR_CREATEDIBSECTION,
                                GetSystemMetrics(SM_CXSMICON),
                                GetSystemMetrics(SM_CYSMICON));
        imagelist_.AddIcon(icon);

        icon = AtlLoadIconImage(IDI_USER_DISABLED,
                                LR_CREATEDIBSECTION,
                                GetSystemMetrics(SM_CXSMICON),
                                GetSystemMetrics(SM_CYSMICON));
        imagelist_.AddIcon(icon);
    }

    CListViewCtrl list(GetDlgItem(IDC_USER_LIST));

    DWORD ex_style = LVS_EX_FULLROWSELECT;

    if (IsWindowsVistaOrGreater())
    {
        SetWindowTheme(list, L"explorer", nullptr);
        ex_style |= LVS_EX_DOUBLEBUFFER;
    }

    list.SetExtendedListViewStyle(ex_style);

    list.SetImageList(imagelist_, LVSIL_SMALL);

    int column_index = list.AddColumn(L"", 0);

    CRect list_rect;
    list.GetClientRect(list_rect);
    list.SetColumnWidth(column_index, list_rect.Width() - GetSystemMetrics(SM_CXVSCROLL));

    if (!IsCallerHasAdminRights())
    {
        GetDlgItem(IDC_USER_LIST).EnableWindow(FALSE);
        GetDlgItem(ID_ADD).EnableWindow(FALSE);
    }

    GetDlgItem(ID_EDIT).EnableWindow(FALSE);
    GetDlgItem(ID_DELETE).EnableWindow(FALSE);

    if (user_list_.LoadFromStorage())
        UpdateUserList();

    return 0;
}

LRESULT UiUsersDialog::OnClose(UINT message,
                               WPARAM wparam,
                               LPARAM lparam,
                               BOOL& handled)
{
    EndDialog(IDCANCEL);
    return 0;
}

LRESULT UiUsersDialog::OnAddButton(WORD notify_code,
                                   WORD control_id,
                                   HWND control,
                                   BOOL& handled)
{
    std::unique_ptr<proto::HostUser> user(new proto::HostUser());

    UiUserPropDialog dialog(UiUserPropDialog::Mode::Add, user.get(), user_list_);
    if (dialog.DoModal(*this) == IDOK)
    {
        user_list_.Add(std::move(user));
        UpdateUserList();
        SetUserListModified();
    }

    GetDlgItem(ID_EDIT).EnableWindow(FALSE);
    GetDlgItem(ID_DELETE).EnableWindow(FALSE);

    return 0;
}

int UiUsersDialog::GetSelectedUserIndex()
{
    CListViewCtrl list(GetDlgItem(IDC_USER_LIST));
    return list.GetItemData(list.GetNextItem(-1, LVNI_SELECTED));
}

void UiUsersDialog::EditSelectedUser()
{
    int user_index = GetSelectedUserIndex();

    if (user_index < 0 || user_index >= user_list_.size())
        return;

    proto::HostUser* user = user_list_.mutable_host_user(user_index);

    UiUserPropDialog dialog(UiUserPropDialog::Mode::Edit, user, user_list_);
    if (dialog.DoModal(*this) == IDOK)
    {
        UpdateUserList();
        SetUserListModified();
    }

    GetDlgItem(ID_EDIT).EnableWindow(FALSE);
    GetDlgItem(ID_DELETE).EnableWindow(FALSE);
}

LRESULT UiUsersDialog::OnEditButton(WORD notify_code,
                                    WORD control_id,
                                    HWND control,
                                    BOOL& handled)
{
    EditSelectedUser();
    return 0;
}

void UiUsersDialog::DeleteSelectedUser()
{
    int user_index = GetSelectedUserIndex();

    if (user_index < 0 || user_index >= user_list_.size())
        return;

    SecureString<std::wstring> username;
    CHECK(UTF8toUNICODE(user_list_.host_user(user_index).username(), username));

    CString title;
    title.LoadStringW(IDS_CONFIRMATION);

    CString message;
    message.Format(IDS_DELETE_USER_CONFORMATION, username.c_str());

    if (MessageBoxW(message, title, MB_YESNO | MB_ICONQUESTION) == IDYES)
    {
        user_list_.Delete(user_index);
        UpdateUserList();
        SetUserListModified();
    }

    GetDlgItem(ID_EDIT).EnableWindow(FALSE);
    GetDlgItem(ID_DELETE).EnableWindow(FALSE);
}

LRESULT UiUsersDialog::OnDeleteButton(WORD notify_code,
                                      WORD control_id,
                                      HWND control,
                                      BOOL& handled)
{
    DeleteSelectedUser();
    return 0;
}

LRESULT UiUsersDialog::OnOkButton(WORD notify_code,
                                  WORD control_id,
                                  HWND control,
                                  BOOL& handled)
{
    user_list_.SaveToStorage();
    EndDialog(IDOK);
    return 0;
}

LRESULT UiUsersDialog::OnCancelButton(WORD notify_code,
                                      WORD control_id,
                                      HWND control,
                                      BOOL& handled)
{
    EndDialog(IDCANCEL);
    return 0;
}

void UiUsersDialog::ShowUserPopupMenu()
{
    CMenu menu(AtlLoadMenu(IDR_USER));

    if (menu)
    {
        POINT cursor_pos;

        if (GetCursorPos(&cursor_pos))
        {
            SetForegroundWindow(*this);

            CMenuHandle popup_menu(menu.GetSubMenu(0));
            if (popup_menu)
            {
                if (GetSelectedUserIndex() == -1)
                {
                    popup_menu.EnableMenuItem(ID_EDIT, MF_BYCOMMAND | MF_DISABLED);
                    popup_menu.EnableMenuItem(ID_DELETE, MF_BYCOMMAND | MF_DISABLED);
                }

                popup_menu.TrackPopupMenu(0, cursor_pos.x, cursor_pos.y, *this, nullptr);
            }
        }
    }

    GetDlgItem(ID_EDIT).EnableWindow(FALSE);
    GetDlgItem(ID_DELETE).EnableWindow(FALSE);
}

void UiUsersDialog::SetUserListModified()
{
    CString text;
    text.LoadStringW(IDS_USER_LIST_MODIFIED);

    CWindow group(GetDlgItem(IDC_USERS_GROUPBOX));
    group.SetWindowTextW(text);
}

LRESULT UiUsersDialog::OnUserListDoubleClick(int control_id,
                                             LPNMHDR hdr,
                                             BOOL& handled)
{
    EditSelectedUser();
    return 0;
}

LRESULT UiUsersDialog::OnUserListRightClick(int control_id,
                                            LPNMHDR hdr,
                                            BOOL& handled)
{
    ShowUserPopupMenu();
    return 0;
}

void UiUsersDialog::OnUserSelect()
{
    if (GetSelectedUserIndex() == -1)
    {
        GetDlgItem(ID_EDIT).EnableWindow(FALSE);
        GetDlgItem(ID_DELETE).EnableWindow(FALSE);
    }
    else
    {
        GetDlgItem(ID_EDIT).EnableWindow(TRUE);
        GetDlgItem(ID_DELETE).EnableWindow(TRUE);
    }
}

LRESULT UiUsersDialog::OnUserListClick(int control_id,
                                       LPNMHDR hdr,
                                       BOOL& handled)
{
    OnUserSelect();
    return 0;
}

LRESULT UiUsersDialog::OnUserListKeyDown(int control_id,
                                         LPNMHDR hdr,
                                         BOOL& handled)
{
    LPNMLVKEYDOWN keydown_header = reinterpret_cast<LPNMLVKEYDOWN>(hdr);

    switch (keydown_header->wVKey)
    {
        case VK_DELETE:
            DeleteSelectedUser();
            break;

        case VK_UP:
        case VK_DOWN:
            OnUserSelect();
            break;
    }

    return 0;
}

} // namespace aspia
