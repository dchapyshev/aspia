//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/users_dialog.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/users_dialog.h"
#include "ui/base/listview.h"
#include "ui/user_prop_dialog.h"
#include "ui/resource.h"
#include "base/process_helpers.h"
#include "base/unicode.h"
#include "base/string_util.h"
#include "host/host_user_utils.h"

namespace aspia {

INT_PTR UsersDialog::DoModal(HWND parent)
{
    return Run(Module::Current(), parent, IDD_USERS);
}

void UsersDialog::UpdateUserList()
{
    ListView list(GetDlgItem(IDC_USER_LIST));

    list.DeleteAllItems();

    for (int i = 0; i < user_list_.user_list_size(); ++i)
    {
        const proto::HostUser& user = user_list_.user_list(i);

        std::wstring username;
        CHECK(UTF8toUNICODE(user.username(), username));

        list.AddItem(username, i, user.enabled() ? 0 : 1);
    }
}

void UsersDialog::OnInitDialog()
{
    int width = GetSystemMetrics(SM_CXSMICON);
    int height = GetSystemMetrics(SM_CYSMICON);

    if (imagelist_.Create(width, height, ILC_MASK | ILC_COLOR32, 1, 1))
    {
        imagelist_.AddIcon(module(), IDI_USER, width, height);
        imagelist_.AddIcon(module(), IDI_USER_DISABLED, width, height);
    }

    ListView list(GetDlgItem(IDC_USER_LIST));

    list.ModifyExtendedListViewStyle(0, LVS_EX_FULLROWSELECT);
    list.SetImageList(imagelist_, LVSIL_SMALL);
    list.AddOnlyOneColumn();

    if (!IsCallerHasAdminRights())
    {
        EnableWindow(GetDlgItem(IDC_USER_LIST), FALSE);
        EnableWindow(GetDlgItem(ID_ADD), FALSE);
    }

    EnableWindow(GetDlgItem(ID_EDIT), FALSE);
    EnableWindow(GetDlgItem(ID_DELETE), FALSE);

    if (ReadUserList(user_list_))
        UpdateUserList();
}

void UsersDialog::OnClose()
{
    EndDialog(IDCANCEL);
}

void UsersDialog::OnAddButton()
{
    std::unique_ptr<proto::HostUser> user(new proto::HostUser());

    UserPropDialog dialog(UserPropDialog::Mode::Add, user.get());
    if (dialog.DoModal(hwnd()) == IDOK)
    {
        user_list_.mutable_user_list()->AddAllocated(user.release());
        UpdateUserList();
        SetUserListModified();
    }

    EnableWindow(GetDlgItem(ID_EDIT), FALSE);
    EnableWindow(GetDlgItem(ID_DELETE), FALSE);
}

int UsersDialog::GetSelectedUserIndex()
{
    ListView list(GetDlgItem(IDC_USER_LIST));
    return list.GetItemData<int>(list.GetFirstSelectedItem());
}

void UsersDialog::OnEditButton()
{
    int user_index = GetSelectedUserIndex();

    if (user_index < 0 || user_index >= user_list_.user_list_size())
        return;

    proto::HostUser* user = user_list_.mutable_user_list(user_index);

    UserPropDialog dialog(UserPropDialog::Mode::Edit, user);
    if (dialog.DoModal(hwnd()) == IDOK)
    {
        UpdateUserList();
        SetUserListModified();
    }

    EnableWindow(GetDlgItem(ID_EDIT), FALSE);
    EnableWindow(GetDlgItem(ID_DELETE), FALSE);
}

void UsersDialog::OnDeleteButton()
{
    int user_index = GetSelectedUserIndex();

    if (user_index < 0 || user_index >= user_list_.user_list_size())
        return;

    std::wstring title = module().string(IDS_CONFIRMATION);
    std::wstring message_format = module().string(IDS_DELETE_USER_CONFORMATION);

    std::wstring username;
    CHECK(UTF8toUNICODE(user_list_.user_list(user_index).username(), username));

    std::wstring message = StringPrintfW(message_format.c_str(), username.c_str());

    if (MessageBoxW(hwnd(), message.c_str(), title.c_str(), MB_YESNO | MB_ICONQUESTION) == IDYES)
    {
        user_list_.mutable_user_list()->DeleteSubrange(user_index, 1);
        UpdateUserList();
        SetUserListModified();
    }

    EnableWindow(GetDlgItem(ID_EDIT), FALSE);
    EnableWindow(GetDlgItem(ID_DELETE), FALSE);
}

void UsersDialog::OnOkButton()
{
    WriteUserList(user_list_);
    EndDialog(IDOK);
}

void UsersDialog::OnCancelButton()
{
    PostMessageW(hwnd(), WM_CLOSE, 0, 0);
}

void UsersDialog::ShowUserPopupMenu()
{
    ScopedHMENU menu(module().menu(IDR_USER));

    if (menu.IsValid())
    {
        POINT cursor_pos;

        if (GetCursorPos(&cursor_pos))
        {
            SetForegroundWindow(hwnd());

            HMENU popup_menu = GetSubMenu(menu, 0);
            if (popup_menu)
            {
                if (GetSelectedUserIndex() == -1)
                {
                    EnableMenuItem(popup_menu, ID_EDIT, MF_BYCOMMAND | MF_DISABLED);
                    EnableMenuItem(popup_menu, ID_DELETE, MF_BYCOMMAND | MF_DISABLED);
                }

                TrackPopupMenu(popup_menu, 0, cursor_pos.x, cursor_pos.y, 0, hwnd(), nullptr);
            }
        }
    }

    EnableWindow(GetDlgItem(ID_EDIT), FALSE);
    EnableWindow(GetDlgItem(ID_DELETE), FALSE);
}

void UsersDialog::OnUserListClicked()
{
    if (GetSelectedUserIndex() == -1)
    {
        EnableWindow(GetDlgItem(ID_EDIT), FALSE);
        EnableWindow(GetDlgItem(ID_DELETE), FALSE);
    }
    else
    {
        EnableWindow(GetDlgItem(ID_EDIT), TRUE);
        EnableWindow(GetDlgItem(ID_DELETE), TRUE);
    }
}

void UsersDialog::SetUserListModified()
{
    HWND group = GetDlgItem(IDC_USERS_GROUPBOX);
    SetWindowTextW(group, module().string(IDS_USER_LIST_MODIFIED).c_str());
}

INT_PTR UsersDialog::OnMessage(UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
        case WM_INITDIALOG:
            OnInitDialog();
            break;

        case WM_NOTIFY:
        {
            LPNMHDR header = reinterpret_cast<LPNMHDR>(lparam);

            if (header->idFrom != IDC_USER_LIST)
                break;

            switch (header->code)
            {
                case NM_DBLCLK:
                    OnEditButton();
                    break;

                case NM_RCLICK:
                    ShowUserPopupMenu();
                    break;

                case NM_CLICK:
                    OnUserListClicked();
                    break;

                case LVN_KEYDOWN:
                {
                    LPNMLVKEYDOWN keydown_header = reinterpret_cast<LPNMLVKEYDOWN>(header);

                    switch (keydown_header->wVKey)
                    {
                        case VK_DELETE:
                            OnDeleteButton();
                            break;

                        case VK_UP:
                        case VK_DOWN:
                            OnUserListClicked();
                            break;
                    }
                }
                break;
            }
        }
        break;

        case WM_COMMAND:
        {
            switch (LOWORD(wparam))
            {
                case ID_ADD:
                    OnAddButton();
                    break;

                case ID_EDIT:
                    OnEditButton();
                    break;

                case ID_DELETE:
                    OnDeleteButton();
                    break;

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
