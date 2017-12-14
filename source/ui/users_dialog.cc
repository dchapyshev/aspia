//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/users_dialog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/users_dialog.h"
#include "ui/user_prop_dialog.h"
#include "base/process/process_helpers.h"
#include "base/strings/unicode.h"
#include "base/version_helpers.h"
#include "base/logging.h"
#include "crypto/secure_memory.h"

#include <atlmisc.h>
#include <uxtheme.h>

namespace aspia {

void UsersDialog::UpdateUserList()
{
    CListViewCtrl list(GetDlgItem(IDC_USER_LIST));

    list.DeleteAllItems();

    const int size = user_list_.size();

    for (int i = 0; i < size; ++i)
    {
        const proto::HostUser& user = user_list_.host_user(i);

        std::wstring username;
        CHECK(UTF8toUNICODE(user.username(), username));

        const int item_index = list.AddItem(list.GetItemCount(),
                                            0,
                                            username.c_str(),
                                            user.enabled() ? 0 : 1);

        list.SetItemData(item_index, i);
    }

    UpdateButtonsState();
}

LRESULT UsersDialog::OnInitDialog(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    DlgResize_Init();

    const CSize small_icon_size(GetSystemMetrics(SM_CXSMICON),
                                GetSystemMetrics(SM_CYSMICON));

    add_icon_ = AtlLoadIconImage(IDI_PLUS,
                                 LR_CREATEDIBSECTION,
                                 small_icon_size.cx,
                                 small_icon_size.cy);
    CButton(GetDlgItem(ID_ADD)).SetIcon(add_icon_);

    edit_icon_ = AtlLoadIconImage(IDI_PENCIL,
                                  LR_CREATEDIBSECTION,
                                  small_icon_size.cx,
                                  small_icon_size.cy);
    CButton(GetDlgItem(ID_EDIT)).SetIcon(edit_icon_);

    delete_icon_ = AtlLoadIconImage(IDI_MINUS,
                                    LR_CREATEDIBSECTION,
                                    small_icon_size.cx,
                                    small_icon_size.cy);
    CButton(GetDlgItem(ID_DELETE)).SetIcon(delete_icon_);

    if (imagelist_.Create(small_icon_size.cx,
                          small_icon_size.cy,
                          ILC_MASK | ILC_COLOR32,
                          1, 1))
    {
        CIcon icon;

        icon = AtlLoadIconImage(IDI_USER,
                                LR_CREATEDIBSECTION,
                                small_icon_size.cx,
                                small_icon_size.cy);
        imagelist_.AddIcon(icon);

        icon = AtlLoadIconImage(IDI_USER_DISABLED,
                                LR_CREATEDIBSECTION,
                                small_icon_size.cx,
                                small_icon_size.cy);
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

    CString title;
    title.LoadStringW(IDS_USER_LIST);
    list.AddColumn(title, 0);

    if (!IsCallerHasAdminRights())
    {
        GetDlgItem(IDC_USER_LIST).EnableWindow(FALSE);
        GetDlgItem(ID_ADD).EnableWindow(FALSE);
    }

    if (user_list_.LoadFromStorage())
        UpdateUserList();

    return TRUE;
}

LRESULT UsersDialog::OnClose(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    EndDialog(IDCANCEL);
    return 0;
}

LRESULT UsersDialog::OnSize(UINT message, WPARAM wparam, LPARAM lparam, BOOL& /* handled */)
{
    LRESULT ret = 0;

    if (CDialogResize<UsersDialog>::ProcessWindowMessage(
        *this, message, wparam, lparam, ret))
    {
        CListViewCtrl list(GetDlgItem(IDC_USER_LIST));

        CRect list_rect;
        list.GetClientRect(list_rect);
        list.SetColumnWidth(0, list_rect.Width() - GetSystemMetrics(SM_CXVSCROLL));
    }

    return ret;
}

LRESULT UsersDialog::OnAddButton(
    WORD /* code */, WORD /* ctrl_id */, HWND /* ctrl */, BOOL& /* handled */)
{
    std::unique_ptr<proto::HostUser> user(std::make_unique<proto::HostUser>());

    UserPropDialog dialog(UserPropDialog::Mode::ADD, user.get(), user_list_);
    if (dialog.DoModal(*this) == IDOK)
    {
        user_list_.Add(std::move(user));
        UpdateUserList();
        SetUserListModified();
    }

    return 0;
}

int UsersDialog::GetSelectedUserIndex()
{
    CListViewCtrl list(GetDlgItem(IDC_USER_LIST));
    return static_cast<int>(list.GetItemData(list.GetNextItem(-1, LVNI_SELECTED)));
}

void UsersDialog::EditSelectedUser()
{
    const int user_index = GetSelectedUserIndex();

    if (user_index < 0 || user_index >= user_list_.size())
        return;

    proto::HostUser* user = user_list_.mutable_host_user(user_index);

    UserPropDialog dialog(UserPropDialog::Mode::EDIT, user, user_list_);
    if (dialog.DoModal(*this) == IDOK)
    {
        UpdateUserList();
        SetUserListModified();
    }
}

LRESULT UsersDialog::OnEditButton(
    WORD /* code */, WORD /* ctrl_id */, HWND /* ctrl */, BOOL& /* handled */)
{
    EditSelectedUser();
    return 0;
}

void UsersDialog::DeleteSelectedUser()
{
    int user_index = GetSelectedUserIndex();

    if (user_index < 0 || user_index >= user_list_.size())
        return;

    std::wstring username;
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
}

LRESULT UsersDialog::OnDeleteButton(
    WORD /* code */, WORD /* ctrl_id */, HWND /* ctrl */, BOOL& /* handled */)
{
    DeleteSelectedUser();
    return 0;
}

LRESULT UsersDialog::OnOkButton(
    WORD /* code */, WORD /* ctrl_id */, HWND /* ctrl */, BOOL& /* handled */)
{
    user_list_.SaveToStorage();
    EndDialog(IDOK);
    return 0;
}

LRESULT UsersDialog::OnCancelButton(
    WORD /* code */, WORD /* ctrl_id */, HWND /* ctrl */, BOOL& /* handled */)
{
    EndDialog(IDCANCEL);
    return 0;
}

void UsersDialog::ShowUserPopupMenu()
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
                CListViewCtrl list(GetDlgItem(IDC_USER_LIST));

                if (!list.GetSelectedCount())
                {
                    popup_menu.EnableMenuItem(ID_EDIT, MF_BYCOMMAND | MF_DISABLED);
                    popup_menu.EnableMenuItem(ID_DELETE, MF_BYCOMMAND | MF_DISABLED);
                }

                popup_menu.TrackPopupMenu(0, cursor_pos.x, cursor_pos.y, *this, nullptr);
            }
        }
    }
}

void UsersDialog::SetUserListModified()
{
    CString text;
    text.LoadStringW(IDS_USER_LIST_MODIFIED);
    SetWindowTextW(text);
}

LRESULT UsersDialog::OnUserListDoubleClick(
    int /* ctrl_id */, LPNMHDR /* hdr */, BOOL& /* handled */)
{
    EditSelectedUser();
    return 0;
}

LRESULT UsersDialog::OnUserListRightClick(
    int /* ctrl_id */, LPNMHDR /* hdr */, BOOL& /* handled */)
{
    ShowUserPopupMenu();
    UpdateButtonsState();
    return 0;
}

void UsersDialog::UpdateButtonsState()
{
    CListViewCtrl list(GetDlgItem(IDC_USER_LIST));

    if (!list.GetSelectedCount())
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

LRESULT UsersDialog::OnUserListKeyDown(int /* ctrl_id */, LPNMHDR hdr, BOOL& /* handled */)
{
    LPNMLVKEYDOWN keydown_header = reinterpret_cast<LPNMLVKEYDOWN>(hdr);

    switch (keydown_header->wVKey)
    {
        case VK_DELETE:
            DeleteSelectedUser();
            break;

        case VK_UP:
        case VK_DOWN:
            UpdateButtonsState();
            break;

        default:
            break;
    }

    return 0;
}

LRESULT UsersDialog::OnUserListItemChanged(
    int /* ctrl_id */, LPNMHDR /* hdr */, BOOL& /* handled */)
{
    UpdateButtonsState();
    return 0;
}

} // namespace aspia
