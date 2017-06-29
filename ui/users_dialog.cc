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
#include "base/process/process_helpers.h"
#include "base/strings/string_util.h"
#include "base/strings/unicode.h"
#include "base/logging.h"
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

void UiUsersDialog::OnInitDialog()
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

    std::wstring title = Module().String(IDS_CONFIRMATION);
    std::wstring message_format = Module().String(IDS_DELETE_USER_CONFORMATION);

    SecureString<std::wstring> username;
    CHECK(UTF8toUNICODE(user_list_.host_user(user_index).username(), username));

    SecureString<std::wstring> message =
        StringPrintfW(message_format.c_str(), username.c_str());

    if (MessageBoxW(message, title, MB_YESNO | MB_ICONQUESTION) == IDYES)
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
    ScopedHMENU menu(Module().Menu(IDR_USER));

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
    UiWindow group(GetDlgItem(IDC_USERS_GROUPBOX));
    group.SetWindowString(Module().String(IDS_USER_LIST_MODIFIED));
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
