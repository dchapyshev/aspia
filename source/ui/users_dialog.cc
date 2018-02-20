//
// PROJECT:         Aspia
// FILE:            ui/users_dialog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/users_dialog.h"

#include <atlmisc.h>
#include <uxtheme.h>

#include "base/process/process_helpers.h"
#include "base/strings/unicode.h"
#include "base/logging.h"
#include "crypto/secure_memory.h"
#include "ui/user_prop_dialog.h"

namespace aspia {

void UsersDialog::UpdateUserList()
{
    CListViewCtrl list(GetDlgItem(IDC_USER_LIST));

    list.DeleteAllItems();

    for (const auto& user : UsersStorage::Open()->ReadUserList())
    {
        list.AddItem(list.GetItemCount(),
                     0,
                     user.name.c_str(),
                     (user.flags & UsersStorage::USER_FLAG_ENABLED) ? 0 : 1);
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

    ::SetWindowTheme(list, L"explorer", nullptr);
    list.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    list.SetImageList(imagelist_, LVSIL_SMALL);

    CString title;
    title.LoadStringW(IDS_USER_LIST);
    list.AddColumn(title, 0);

    if (!IsCallerHasAdminRights())
    {
        GetDlgItem(IDC_USER_LIST).EnableWindow(FALSE);
        GetDlgItem(ID_ADD).EnableWindow(FALSE);
    }

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
    UsersStorage::User user;

    UserPropDialog dialog(UserPropDialog::Mode::ADD, &user);
    if (dialog.DoModal(*this) == IDOK)
    {
        UsersStorage::Open()->AddUser(user);
        UpdateUserList();
    }

    return 0;
}

std::wstring UsersDialog::GetSelectedUserName()
{
    CListViewCtrl list(GetDlgItem(IDC_USER_LIST));

    wchar_t username[128] = { 0 };
    if (!list.GetItemText(list.GetNextItem(-1, LVNI_SELECTED), 0, username, _countof(username)))
        return std::wstring();

    return username;
}

void UsersDialog::EditSelectedUser()
{
    UsersStorage::User user;
    user.name = GetSelectedUserName();

    std::unique_ptr<UsersStorage> storage = UsersStorage::Open();
    if (!storage->ReadUser(user))
        return;

    UserPropDialog dialog(UserPropDialog::Mode::EDIT, &user);
    if (dialog.DoModal(*this) == IDOK)
    {
        storage->ModifyUser(user);
        UpdateUserList();
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
    std::wstring username = GetSelectedUserName();
    if (username.empty())
        return;

    CString title;
    title.LoadStringW(IDS_CONFIRMATION);

    CString message;
    message.Format(IDS_DELETE_USER_CONFORMATION, username.c_str());

    if (MessageBoxW(message, title, MB_YESNO | MB_ICONQUESTION) == IDYES)
    {
        UsersStorage::Open()->RemoveUser(username);
        UpdateUserList();
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
