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
#include "crypto/secure_string.h"

namespace aspia {

INT_PTR UiUsersDialog::DoModal(HWND parent)
{
    return Run(UiModule::Current(), parent, IDD_USERS);
}

void UiUsersDialog::UpdateUserList()
{
    UiListView list(GetDlgItem(IDC_USER_LIST));

    list.DeleteAllItems();

    const int size = user_list_.size();

    for (int i = 0; i < size; ++i)
    {
        const proto::HostUser& user = user_list_.host_user(i);

        SecureString<std::wstring> username;
        CHECK(UTF8toUNICODE(user.username(), username));

        list.AddItem(username, i, user.enabled() ? 0 : 1);
    }
}

void UiUsersDialog::OnInitDialog()
{
    if (imagelist_.CreateSmall())
    {
        imagelist_.AddIcon(module(), IDI_USER);
        imagelist_.AddIcon(module(), IDI_USER_DISABLED);
    }

    UiListView list(GetDlgItem(IDC_USER_LIST));

    list.ModifyExtendedListViewStyle(0, LVS_EX_FULLROWSELECT);
    list.SetImageList(imagelist_, LVSIL_SMALL);
    list.AddOnlyOneColumn();

    if (!IsCallerHasAdminRights())
    {
        EnableDlgItem(IDC_USER_LIST, FALSE);
        EnableDlgItem(ID_ADD, FALSE);
    }

    EnableDlgItem(ID_EDIT, FALSE);
    EnableDlgItem(ID_DELETE, FALSE);

    if (user_list_.LoadFromStorage())
        UpdateUserList();
}

void UiUsersDialog::OnAddButton()
{
    std::unique_ptr<proto::HostUser> user(new proto::HostUser());

    UiUserPropDialog dialog(UiUserPropDialog::Mode::Add, user.get(), user_list_);
    if (dialog.DoModal(hwnd()) == IDOK)
    {
        user_list_.Add(std::move(user));
        UpdateUserList();
        SetUserListModified();
    }

    EnableDlgItem(ID_EDIT, FALSE);
    EnableDlgItem(ID_DELETE, FALSE);
}

int UiUsersDialog::GetSelectedUserIndex()
{
    UiListView list(GetDlgItem(IDC_USER_LIST));
    return list.GetItemData<int>(list.GetFirstSelectedItem());
}

void UiUsersDialog::OnEditButton()
{
    int user_index = GetSelectedUserIndex();

    if (user_index < 0 || user_index >= user_list_.size())
        return;

    proto::HostUser* user = user_list_.mutable_host_user(user_index);

    UiUserPropDialog dialog(UiUserPropDialog::Mode::Edit, user, user_list_);
    if (dialog.DoModal(hwnd()) == IDOK)
    {
        UpdateUserList();
        SetUserListModified();
    }

    EnableDlgItem(ID_EDIT, FALSE);
    EnableDlgItem(ID_DELETE, FALSE);
}

void UiUsersDialog::OnDeleteButton()
{
    int user_index = GetSelectedUserIndex();

    if (user_index < 0 || user_index >= user_list_.size())
        return;

    std::wstring title = module().string(IDS_CONFIRMATION);
    std::wstring message_format = module().string(IDS_DELETE_USER_CONFORMATION);

    SecureString<std::wstring> username;
    CHECK(UTF8toUNICODE(user_list_.host_user(user_index).username(), username));

    SecureString<std::wstring> message =
        StringPrintfW(message_format.c_str(), username.c_str());

    if (MessageBoxW(hwnd(),
                    message.c_str(),
                    title.c_str(),
                    MB_YESNO | MB_ICONQUESTION) == IDYES)
    {
        user_list_.Delete(user_index);
        UpdateUserList();
        SetUserListModified();
    }

    EnableDlgItem(ID_EDIT, FALSE);
    EnableDlgItem(ID_DELETE, FALSE);
}

void UiUsersDialog::OnOkButton()
{
    user_list_.SaveToStorage();
    EndDialog(IDOK);
}

void UiUsersDialog::ShowUserPopupMenu()
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

    EnableDlgItem(ID_EDIT, FALSE);
    EnableDlgItem(ID_DELETE, FALSE);
}

void UiUsersDialog::OnUserListClicked()
{
    if (GetSelectedUserIndex() == -1)
    {
        EnableDlgItem(ID_EDIT, FALSE);
        EnableDlgItem(ID_DELETE, FALSE);
    }
    else
    {
        EnableDlgItem(ID_EDIT, TRUE);
        EnableDlgItem(ID_DELETE, TRUE);
    }
}

void UiUsersDialog::SetUserListModified()
{
    HWND group = GetDlgItem(IDC_USERS_GROUPBOX);
    SetWindowTextW(group, module().string(IDS_USER_LIST_MODIFIED).c_str());
}

INT_PTR UiUsersDialog::OnMessage(UINT msg, WPARAM wparam, LPARAM lparam)
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
