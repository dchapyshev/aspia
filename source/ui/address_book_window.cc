//
// PROJECT:         Aspia
// FILE:            ui/address_book_window.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/address_book_window.h"

#include <atldlgs.h>
#include <strsafe.h>
#include <fstream>

#include "base/strings/unicode.h"
#include "base/version_helpers.h"
#include "ui/about_dialog.h"
#include "ui/computer_dialog.h"
#include "ui/computer_group_dialog.h"

namespace aspia {

namespace {

const WCHAR kAddressBookFileExtension[] = L"*.aad";

const int kComputerIcon = 0;
const int kAddressBookIcon = 0;
const int kComputerGroupIcon = 1;

} // namespace

bool AddressBookWindow::Dispatch(const NativeEvent& event)
{
    if (!accelerator_.TranslateAcceleratorW(*this, const_cast<NativeEvent*>(&event)))
    {
        TranslateMessage(&event);
        DispatchMessageW(&event);
    }

    return true;
}

LRESULT AddressBookWindow::OnCreate(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    SetWindowPos(nullptr, 0, 0, 980, 700, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
    CenterWindow();

    accelerator_.LoadAcceleratorsW(IDC_ADDRESS_BOOK_ACCELERATORS);

    const CSize small_icon_size(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));

    small_icon_ = AtlLoadIconImage(IDI_MAIN,
                                   LR_CREATEDIBSECTION,
                                   small_icon_size.cx,
                                   small_icon_size.cy);
    SetIcon(small_icon_, FALSE);

    big_icon_ = AtlLoadIconImage(IDI_MAIN,
                                 LR_CREATEDIBSECTION,
                                 GetSystemMetrics(SM_CXICON),
                                 GetSystemMetrics(SM_CYICON));
    SetIcon(big_icon_, TRUE);

    CString title;
    title.LoadStringW(IDS_ADDRESS_BOOK_TITLE);
    SetWindowTextW(title);

    main_menu_ = AtlLoadMenu(IDR_ADDRESS_BOOK_MAIN);

    CMenuHandle file_menu(main_menu_.GetSubMenu(0));
    file_menu.EnableMenuItem(ID_SAVE, MF_BYCOMMAND | MF_DISABLED);
    file_menu.EnableMenuItem(ID_SAVE_AS, MF_BYCOMMAND | MF_DISABLED);
    file_menu.EnableMenuItem(ID_CLOSE, MF_BYCOMMAND | MF_DISABLED);

    CMenuHandle edit_menu(main_menu_.GetSubMenu(1));
    edit_menu.EnableMenuItem(ID_ADD_GROUP, MF_BYCOMMAND | MF_DISABLED);
    edit_menu.EnableMenuItem(ID_DELETE_GROUP, MF_BYCOMMAND | MF_DISABLED);
    edit_menu.EnableMenuItem(ID_EDIT_GROUP, MF_BYCOMMAND | MF_DISABLED);
    edit_menu.EnableMenuItem(ID_ADD_COMPUTER, MF_BYCOMMAND | MF_DISABLED);
    edit_menu.EnableMenuItem(ID_DELETE_COMPUTER, MF_BYCOMMAND | MF_DISABLED);
    edit_menu.EnableMenuItem(ID_EDIT_COMPUTER, MF_BYCOMMAND | MF_DISABLED);

    CMenuHandle session_type_menu(main_menu_.GetSubMenu(2));
    session_type_menu.CheckMenuRadioItem(ID_DESKTOP_MANAGE_SESSION_TB,
                                         ID_POWER_MANAGE_SESSION_TB,
                                         ID_DESKTOP_MANAGE_SESSION_TB,
                                         MF_BYCOMMAND);

    SetMenu(main_menu_);

    statusbar_.Create(*this, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP);
    int part_width = 240;
    statusbar_.SetParts(1, &part_width);

    CRect client_rect;
    GetClientRect(client_rect);

    splitter_.Create(*this, client_rect, nullptr,
                     WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);

    splitter_.SetActivePane(SPLIT_PANE_LEFT);
    splitter_.SetSplitterPos(230);
    splitter_.m_cxySplitBar = 5;
    splitter_.m_cxyMin = 0;
    splitter_.m_bFullDrag = false;

    splitter_.SetSplitterExtendedStyle(splitter_.GetSplitterExtendedStyle() &~SPLIT_PROPORTIONAL);

    InitGroupTree(small_icon_size);
    InitComputerList(small_icon_size);

    splitter_.SetSplitterPane(SPLIT_PANE_LEFT, group_tree_ctrl_);
    splitter_.SetSplitterPane(SPLIT_PANE_RIGHT, computer_list_ctrl_);

    InitToolBar(small_icon_size);

    if (!address_book_)
    {
        group_tree_ctrl_.EnableWindow(FALSE);
        computer_list_ctrl_.EnableWindow(FALSE);
    }

    return 0;
}

LRESULT AddressBookWindow::OnDestroy(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    return 0;
}

LRESULT AddressBookWindow::OnSize(
    UINT /* message */, WPARAM /* wparam */, LPARAM lparam, BOOL& /* handled */)
{
    const CSize size(static_cast<DWORD>(lparam));

    toolbar_.AutoSize();
    statusbar_.SendMessageW(WM_SIZE);

    CRect toolbar_rect;
    toolbar_.GetWindowRect(toolbar_rect);

    CRect statusbar_rect;
    statusbar_.GetWindowRect(statusbar_rect);

    splitter_.MoveWindow(0, toolbar_rect.Height(),
                         size.cx, size.cy - toolbar_rect.Height() - statusbar_rect.Height(),
                         FALSE);
    return 0;
}

LRESULT AddressBookWindow::OnGetMinMaxInfo(
    UINT /* message */, WPARAM /* wparam */, LPARAM lparam, BOOL& /* handled */)
{
    LPMINMAXINFO mmi = reinterpret_cast<LPMINMAXINFO>(lparam);

    mmi->ptMinTrackSize.x = 500;
    mmi->ptMinTrackSize.y = 400;

    return 0;
}

LRESULT AddressBookWindow::OnClose(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    if (address_book_changed_)
    {
        if (!SaveAddressBook(address_book_path_))
            return 0;
    }

    splitter_.DestroyWindow();
    group_tree_ctrl_.DestroyWindow();
    computer_list_ctrl_.DestroyWindow();
    statusbar_.DestroyWindow();
    toolbar_.DestroyWindow();

    DestroyWindow();
    PostQuitMessage(0);

    return 0;
}

LRESULT AddressBookWindow::OnGetDispInfo(int /* control_id */, LPNMHDR hdr, BOOL& /* handled */)
{
    LPNMTTDISPINFOW header = reinterpret_cast<LPNMTTDISPINFOW>(hdr);
    UINT string_id;

    switch (header->hdr.idFrom)
    {
        case ID_NEW:
            string_id = IDS_TOOLTIP_NEW;
            break;

        case ID_OPEN:
            string_id = IDS_TOOLTIP_OPEN;
            break;

        case ID_SAVE:
            string_id = IDS_TOOLTIP_SAVE;
            break;

        case ID_ABOUT:
            string_id = IDS_TOOLTIP_ABOUT;
            break;

        case ID_EXIT:
            string_id = IDS_TOOLTIP_EXIT;
            break;

        case ID_ADD_GROUP:
            string_id = IDS_TOOLTIP_ADD_GROUP;
            break;

        case ID_DELETE_GROUP:
            string_id = IDS_TOOLTIP_DELETE_GROUP;
            break;

        case ID_EDIT_GROUP:
            string_id = IDS_TOOLTIP_EDIT_GROUP;
            break;

        case ID_ADD_COMPUTER:
            string_id = IDS_TOOLTIP_ADD_COMPUTER;
            break;

        case ID_DELETE_COMPUTER:
            string_id = IDS_TOOLTIP_DELETE_COMPUTER;
            break;

        case ID_EDIT_COMPUTER:
            string_id = IDS_TOOLTIP_EDIT_COMPUTER;
            break;

        case ID_DESKTOP_MANAGE_SESSION_TB:
            string_id = IDS_SESSION_TYPE_DESKTOP_MANAGE;
            break;

        case ID_DESKTOP_VIEW_SESSION_TB:
            string_id = IDS_SESSION_TYPE_DESKTOP_VIEW;
            break;

        case ID_FILE_TRANSFER_SESSION_TB:
            string_id = IDS_SESSION_TYPE_FILE_TRANSFER;
            break;

        case ID_SYSTEM_INFO_SESSION_TB:
            string_id = IDS_SESSION_TYPE_SYSTEM_INFO;
            break;

        case ID_POWER_MANAGE_SESSION_TB:
            string_id = IDS_SESSION_TYPE_POWER_MANAGE;
            break;

        default:
            return FALSE;
    }

    AtlLoadString(string_id, header->szText, _countof(header->szText));
    return TRUE;
}

LRESULT AddressBookWindow::OnComputerListDoubleClick(
    int /*control_id */, LPNMHDR /* hdr */, BOOL& /* handled */)
{
    int item_index = computer_list_ctrl_.GetNextItem(-1, LVNI_SELECTED);
    if (item_index == -1)
        return 0;

    proto::Computer* computer = reinterpret_cast<proto::Computer*>(
        computer_list_ctrl_.GetItemData(item_index));

    if (!computer)
        return 0;

    proto::auth::SessionType session_type;

    if (toolbar_.IsButtonChecked(ID_DESKTOP_MANAGE_SESSION_TB))
    {
        session_type = proto::auth::SESSION_TYPE_DESKTOP_MANAGE;
    }
    else if (toolbar_.IsButtonChecked(ID_DESKTOP_VIEW_SESSION_TB))
    {
        session_type = proto::auth::SESSION_TYPE_DESKTOP_VIEW;
    }
    else if (toolbar_.IsButtonChecked(ID_FILE_TRANSFER_SESSION_TB))
    {
        session_type = proto::auth::SESSION_TYPE_FILE_TRANSFER;
    }
    else if (toolbar_.IsButtonChecked(ID_SYSTEM_INFO_SESSION_TB))
    {
        session_type = proto::auth::SESSION_TYPE_SYSTEM_INFO;
    }
    else if (toolbar_.IsButtonChecked(ID_POWER_MANAGE_SESSION_TB))
    {
        session_type = proto::auth::SESSION_TYPE_POWER_MANAGE;
    }
    else
    {
        return 0;
    }

    computer->set_session_type(session_type);
    Connect(*computer);

    return 0;
}

LRESULT AddressBookWindow::OnComputerListRightClick(
    int /* control_id */, LPNMHDR /* hdr */, BOOL& /* handled */)
{
    CMenu menu;

    if (computer_list_ctrl_.GetSelectedCount() != 0)
        menu = AtlLoadMenu(IDR_COMPUTER_LIST_ITEM);
    else
        menu = AtlLoadMenu(IDR_COMPUTER_LIST);

    POINT cursor_pos;
    GetCursorPos(&cursor_pos);

    SetForegroundWindow(*this);

    CMenuHandle popup_menu(menu.GetSubMenu(0));
    popup_menu.TrackPopupMenu(0, cursor_pos.x, cursor_pos.y, *this, nullptr);

    return 0;
}

LRESULT AddressBookWindow::OnComputerListItemChanged(
    int /* control_id */, LPNMHDR /* hdr */, BOOL& /* handled */)
{
    int selected_count = computer_list_ctrl_.GetSelectedCount();

    CMenuHandle edit_menu(main_menu_.GetSubMenu(1));

    if (selected_count <= 0)
    {
        toolbar_.EnableButton(ID_DELETE_COMPUTER, FALSE);
        toolbar_.EnableButton(ID_EDIT_COMPUTER, FALSE);

        edit_menu.EnableMenuItem(ID_DELETE_COMPUTER, MF_BYCOMMAND | MF_DISABLED);
        edit_menu.EnableMenuItem(ID_EDIT_COMPUTER, MF_BYCOMMAND | MF_DISABLED);
    }
    else if (selected_count == 1)
    {
        toolbar_.EnableButton(ID_DELETE_COMPUTER, TRUE);
        toolbar_.EnableButton(ID_EDIT_COMPUTER, TRUE);

        edit_menu.EnableMenuItem(ID_DELETE_COMPUTER, MF_BYCOMMAND | MF_ENABLED);
        edit_menu.EnableMenuItem(ID_EDIT_COMPUTER, MF_BYCOMMAND | MF_ENABLED);
    }
    else
    {
        toolbar_.EnableButton(ID_DELETE_COMPUTER, TRUE);
        toolbar_.EnableButton(ID_EDIT_COMPUTER, FALSE);

        edit_menu.EnableMenuItem(ID_DELETE_COMPUTER, MF_BYCOMMAND | MF_ENABLED);
        edit_menu.EnableMenuItem(ID_EDIT_COMPUTER, MF_BYCOMMAND | MF_DISABLED);
    }

    return 0;
}

LRESULT AddressBookWindow::OnGroupSelected(int /* control_id */, LPNMHDR hdr, BOOL& /* handled */)
{
    LPNMTREEVIEWW header = reinterpret_cast<LPNMTREEVIEWW>(hdr);

    group_tree_edited_item_ = header->itemNew.hItem;

    proto::ComputerGroup* group =
        reinterpret_cast<proto::ComputerGroup*>(header->itemNew.lParam);

    CMenuHandle edit_menu(main_menu_.GetSubMenu(1));

    toolbar_.EnableButton(ID_ADD_GROUP, TRUE);
    toolbar_.EnableButton(ID_ADD_COMPUTER, TRUE);

    edit_menu.EnableMenuItem(ID_ADD_GROUP, MF_BYCOMMAND | MF_ENABLED);
    edit_menu.EnableMenuItem(ID_ADD_COMPUTER, MF_BYCOMMAND | MF_ENABLED);

    if (!group)
    {
        toolbar_.EnableButton(ID_EDIT_GROUP, FALSE);
        toolbar_.EnableButton(ID_DELETE_GROUP, FALSE);

        edit_menu.EnableMenuItem(ID_EDIT_GROUP, MF_BYCOMMAND | MF_DISABLED);
        edit_menu.EnableMenuItem(ID_DELETE_GROUP, MF_BYCOMMAND | MF_DISABLED);

        group = address_book_->mutable_root_group();
    }
    else
    {
        toolbar_.EnableButton(ID_EDIT_GROUP, TRUE);
        toolbar_.EnableButton(ID_DELETE_GROUP, TRUE);

        edit_menu.EnableMenuItem(ID_EDIT_GROUP, MF_BYCOMMAND | MF_ENABLED);
        edit_menu.EnableMenuItem(ID_DELETE_GROUP, MF_BYCOMMAND | MF_ENABLED);
    }

    computer_list_ctrl_.DeleteAllItems();
    AddChildComputers(group);

    return 0;
}

LRESULT AddressBookWindow::OnGroupTreeRightClick(
    int /* control_id */, LPNMHDR /* hdr */, BOOL& /* handled */)
{
    POINT cursor_pos;
    GetCursorPos(&cursor_pos);

    group_tree_ctrl_.GetParent().ScreenToClient(&cursor_pos);

    TVHITTESTINFO hit_test_info;
    hit_test_info.pt.x = cursor_pos.x;
    hit_test_info.pt.y = cursor_pos.y;

    group_tree_edited_item_ = group_tree_ctrl_.HitTest(&hit_test_info);
    if (!group_tree_edited_item_)
        return 0;

    CMenu menu;

    // First visible item is always the title of the address book.
    if (group_tree_edited_item_ != group_tree_ctrl_.GetFirstVisibleItem())
    {
        // Load menu for selected group.
        menu = AtlLoadMenu(IDR_GROUP_TREE_ITEM);
    }
    else
    {
        // Load menu for address book title.
        menu = AtlLoadMenu(IDR_GROUP_TREE);
    }

    group_tree_ctrl_.GetParent().ClientToScreen(&cursor_pos);

    SetForegroundWindow(*this);

    CMenuHandle popup_menu(menu.GetSubMenu(0));
    popup_menu.TrackPopupMenu(0, cursor_pos.x, cursor_pos.y, *this, nullptr);

    return 0;
}

LRESULT AddressBookWindow::OnGroupTreeItemExpanded(
    int /* control_id */, LPNMHDR hdr, BOOL& /* handled */)
{
    LPNMTREEVIEWW pnmtv = reinterpret_cast<LPNMTREEVIEWW>(hdr);

    proto::ComputerGroup* group = reinterpret_cast<proto::ComputerGroup*>(pnmtv->itemNew.lParam);
    if (group)
    {
        group->set_expanded((pnmtv->action == TVE_EXPAND) ? true : false);
        SetAddressBookChanged(true);
    }

    return 0;
}

LRESULT AddressBookWindow::OnOpenButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    OpenAddressBook();
    return 0;
}

LRESULT AddressBookWindow::OnSaveButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    if (address_book_changed_)
        SaveAddressBook(address_book_path_);
    return 0;
}

LRESULT AddressBookWindow::OnSaveAsButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    if (address_book_changed_)
        SaveAddressBook(std::experimental::filesystem::path());
    return 0;
}

LRESULT AddressBookWindow::OnAboutButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    AboutDialog().DoModal();
    return 0;
}

LRESULT AddressBookWindow::OnExitButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    SendMessageW(WM_CLOSE);
    return 0;
}

LRESULT AddressBookWindow::OnNewButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    if (!CloseAddressBook())
        return 0;

    computer_list_ctrl_.EnableWindow(TRUE);
    group_tree_ctrl_.EnableWindow(TRUE);

    CString name;
    name.LoadStringW(IDS_DEFAULT_ADDRESS_BOOK_NAME);

    group_tree_ctrl_.InsertItem(name, kAddressBookIcon, kAddressBookIcon, TVI_ROOT, TVI_LAST);

    address_book_ = std::make_unique<proto::AddressBook>();
    address_book_->mutable_root_group()->set_name(UTF8fromUNICODE(name));

    SetAddressBookChanged(true);

    return 0;
}

LRESULT AddressBookWindow::OnCloseButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    CloseAddressBook();
    return 0;
}

LRESULT AddressBookWindow::OnAddComputerButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    DCHECK(group_tree_edited_item_ != nullptr);

    ComputerDialog dialog;

    if (dialog.DoModal() == IDOK)
    {
        proto::ComputerGroup* parent_group =
            reinterpret_cast<proto::ComputerGroup*>(
                group_tree_ctrl_.GetItemData(group_tree_edited_item_));

        proto::Computer* computer;

        if (!parent_group)
            computer = address_book_->mutable_root_group()->add_computer();
        else
            computer = parent_group->add_computer();

        computer->CopyFrom(dialog.GetComputer());

        if (group_tree_edited_item_ != group_tree_ctrl_.GetSelectedItem())
        {
            group_tree_ctrl_.SelectItem(group_tree_edited_item_);
        }
        else
        {
            int item_index = computer_list_ctrl_.AddItem(
                computer_list_ctrl_.GetItemCount(),
                0,
                UNICODEfromUTF8(computer->name()).c_str(), kComputerIcon);

            computer_list_ctrl_.SetItemData(item_index, reinterpret_cast<DWORD_PTR>(computer));

            computer_list_ctrl_.SetItemText(
                item_index, 1, UNICODEfromUTF8(computer->address()).c_str());

            computer_list_ctrl_.SetItemText(
                item_index, 2, std::to_wstring(computer->port()).c_str());
        }

        SetAddressBookChanged(true);
    }

    return 0;
}

LRESULT AddressBookWindow::OnAddGroupButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    if (!group_tree_edited_item_)
        group_tree_edited_item_ = group_tree_ctrl_.GetSelectedItem();

    if (!group_tree_edited_item_)
        return 0;

    ComputerGroupDialog dialog;

    if (dialog.DoModal() == IDOK)
    {
        proto::ComputerGroup* parent_group =
            reinterpret_cast<proto::ComputerGroup*>(
                group_tree_ctrl_.GetItemData(group_tree_edited_item_));

        proto::ComputerGroup* new_group;

        if (!parent_group)
            new_group = address_book_->mutable_root_group()->add_group();
        else
            new_group = parent_group->add_group();

        new_group->CopyFrom(dialog.GetComputerGroup());

        HTREEITEM item = group_tree_ctrl_.InsertItem(UNICODEfromUTF8(new_group->name()).c_str(),
                                                     kComputerGroupIcon,
                                                     kComputerGroupIcon,
                                                     group_tree_edited_item_,
                                                     TVI_LAST);
        group_tree_ctrl_.SetItemData(item, reinterpret_cast<DWORD_PTR>(new_group));
        group_tree_ctrl_.Expand(group_tree_edited_item_, TVE_EXPAND);
        group_tree_ctrl_.SelectItem(item);

        SetAddressBookChanged(true);
    }

    return 0;
}

LRESULT AddressBookWindow::OnEditComputerButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    int item_index = computer_list_ctrl_.GetNextItem(-1, LVNI_SELECTED);
    if (item_index == -1)
        return 0;

    proto::Computer* computer = reinterpret_cast<proto::Computer*>(
        computer_list_ctrl_.GetItemData(item_index));

    if (!computer)
        return 0;

    ComputerDialog dialog(*computer);

    if (dialog.DoModal() == IDOK)
    {
        computer->CopyFrom(dialog.GetComputer());

        computer_list_ctrl_.SetItemText(item_index, 0,
            UNICODEfromUTF8(computer->name()).c_str());

        computer_list_ctrl_.SetItemText(item_index, 1,
            UNICODEfromUTF8(computer->address()).c_str());

        computer_list_ctrl_.SetItemText(item_index, 2,
            std::to_wstring(computer->port()).c_str());

        SetAddressBookChanged(true);
    }

    return 0;
}

LRESULT AddressBookWindow::OnEditGroupButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    if (!group_tree_edited_item_)
        group_tree_edited_item_ = group_tree_ctrl_.GetSelectedItem();

    if (!group_tree_edited_item_)
        return 0;

    proto::ComputerGroup* selected_group =
        reinterpret_cast<proto::ComputerGroup*>(
            group_tree_ctrl_.GetItemData(group_tree_edited_item_));
    if (!selected_group)
        return 0;

    ComputerGroupDialog dialog(*selected_group);

    if (dialog.DoModal() == IDOK)
    {
        selected_group->CopyFrom(dialog.GetComputerGroup());

        group_tree_ctrl_.SetItemText(
            group_tree_edited_item_, UNICODEfromUTF8(selected_group->name()).c_str());

        SetAddressBookChanged(true);
    }

    return 0;
}

LRESULT AddressBookWindow::OnDeleteComputerButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    int item_index = computer_list_ctrl_.GetNextItem(-1, LVNI_SELECTED);
    if (item_index == -1)
        return 0;

    proto::Computer* computer = reinterpret_cast<proto::Computer*>(
        computer_list_ctrl_.GetItemData(item_index));

    if (!computer)
        return 0;

    CString title;
    title.LoadStringW(IDS_CONFIRMATION);

    CString message;
    message.Format(IDS_COMPUTER_DELETE_CONFIRMATION, UNICODEfromUTF8(computer->name()).c_str());

    if (MessageBoxW(message, title, MB_YESNO | MB_ICONQUESTION) == IDYES)
    {
        HTREEITEM parent_group_item = group_tree_ctrl_.GetSelectedItem();
        if (!parent_group_item)
            return 0;

        proto::ComputerGroup* parent_group = reinterpret_cast<proto::ComputerGroup*>(
            group_tree_ctrl_.GetItemData(parent_group_item));
        if (!parent_group)
            return 0;

        computer_list_ctrl_.DeleteItem(item_index);

        for (int i = 0; i < parent_group->computer_size(); ++i)
        {
            if (parent_group->mutable_computer(i) == computer)
            {
                parent_group->mutable_computer()->DeleteSubrange(i, 1);
                break;
            }
        }

        SetAddressBookChanged(true);
    }

    return 0;
}

LRESULT AddressBookWindow::OnDeleteGroupButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    if (!group_tree_edited_item_)
        group_tree_edited_item_ = group_tree_ctrl_.GetSelectedItem();

    if (!group_tree_edited_item_)
        return 0;

    proto::ComputerGroup* selected_group =
        reinterpret_cast<proto::ComputerGroup*>(
            group_tree_ctrl_.GetItemData(group_tree_edited_item_));
    if (!selected_group)
        return 0;

    CString title;
    title.LoadStringW(IDS_CONFIRMATION);

    CString message;
    message.Format(IDS_GROUP_DELETE_CONFIRMATION, UNICODEfromUTF8(selected_group->name()).c_str());

    if (MessageBoxW(message, title, MB_YESNO | MB_ICONQUESTION) == IDYES)
    {
        HTREEITEM parent_item = group_tree_ctrl_.GetParentItem(group_tree_edited_item_);
        if (!parent_item)
            return 0;

        group_tree_ctrl_.DeleteItem(group_tree_edited_item_);

        proto::ComputerGroup* parent_group =
            reinterpret_cast<proto::ComputerGroup*>(group_tree_ctrl_.GetItemData(parent_item));
        if (!parent_group)
            parent_group = address_book_->mutable_root_group();

        for (int i = 0; i < parent_group->group_size(); ++i)
        {
            if (parent_group->mutable_group(i) == selected_group)
            {
                parent_group->mutable_group()->DeleteSubrange(i, 1);
                break;
            }
        }

        SetAddressBookChanged(true);
    }

    return 0;
}

LRESULT AddressBookWindow::OnOpenSessionButton(
    WORD /* notify_code */, WORD control_id, HWND /* control */, BOOL& /* handled */)
{
    int item_index = computer_list_ctrl_.GetNextItem(-1, LVNI_SELECTED);
    if (item_index == -1)
        return 0;

    proto::Computer* computer = reinterpret_cast<proto::Computer*>(
        computer_list_ctrl_.GetItemData(item_index));

    if (!computer)
        return 0;

    proto::auth::SessionType session_type;

    switch (control_id)
    {
        case ID_DESKTOP_MANAGE_SESSION:
            session_type = proto::auth::SESSION_TYPE_DESKTOP_MANAGE;
            break;

        case ID_DESKTOP_VIEW_SESSION:
            session_type = proto::auth::SESSION_TYPE_DESKTOP_VIEW;
            break;

        case ID_FILE_TRANSFER_SESSION:
            session_type = proto::auth::SESSION_TYPE_FILE_TRANSFER;
            break;

        case ID_SYSTEM_INFO_SESSION:
            session_type = proto::auth::SESSION_TYPE_SYSTEM_INFO;
            break;

        case ID_POWER_MANAGE_SESSION:
            session_type = proto::auth::SESSION_TYPE_POWER_MANAGE;
            break;

        default:
            return 0;
    }

    computer->set_session_type(session_type);

    Connect(*computer);

    return 0;
}

LRESULT AddressBookWindow::OnSelectSessionButton(
    WORD /* notify_code */, WORD control_id, HWND /* control */, BOOL& /* handled */)
{
    CMenuHandle session_type_menu(main_menu_.GetSubMenu(2));

    session_type_menu.CheckMenuRadioItem(ID_DESKTOP_MANAGE_SESSION_TB,
                                         ID_POWER_MANAGE_SESSION_TB,
                                         control_id,
                                         MF_BYCOMMAND);

    toolbar_.CheckButton(control_id, TRUE);
    return 0;
}

void AddressBookWindow::InitToolBar(const CSize& small_icon_size)
{
    const DWORD kStyle = WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS;

    toolbar_.Create(*this, rcDefault, nullptr, kStyle);
    toolbar_.SetExtendedStyle(TBSTYLE_EX_DOUBLEBUFFER);

    const BYTE kSessionButtonStyle = BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_CHECKGROUP;
    const BYTE kButtonStyle = BTNS_BUTTON | BTNS_AUTOSIZE;

    TBBUTTON kButtons[] =
    {
        // iBitmap, idCommand, fsState, fsStyle, bReserved[2], dwData, iString
        {  0, ID_NEW,                       TBSTATE_ENABLED,                   kButtonStyle,        { 0 }, 0, -1 },
        {  1, ID_OPEN,                      TBSTATE_ENABLED,                   kButtonStyle,        { 0 }, 0, -1 },
        {  2, ID_SAVE,                      0,                                 kButtonStyle,        { 0 }, 0, -1 },
        { -1, 0,                            TBSTATE_ENABLED,                   BTNS_SEP,            { 0 }, 0, -1 },
        {  3, ID_ADD_GROUP,                 0,                                 kButtonStyle,        { 0 }, 0, -1 },
        {  4, ID_DELETE_GROUP,              0,                                 kButtonStyle,        { 0 }, 0, -1 },
        {  5, ID_EDIT_GROUP,                0,                                 kButtonStyle,        { 0 }, 0, -1 },
        {  6, ID_ADD_COMPUTER,              0,                                 kButtonStyle,        { 0 }, 0, -1 },
        {  7, ID_DELETE_COMPUTER,           0,                                 kButtonStyle,        { 0 }, 0, -1 },
        {  8, ID_EDIT_COMPUTER,             0,                                 kButtonStyle,        { 0 }, 0, -1 },
        { -1, 0,                            TBSTATE_ENABLED,                   BTNS_SEP,            { 0 }, 0, -1 },
        {  9, ID_DESKTOP_MANAGE_SESSION_TB, TBSTATE_ENABLED | TBSTATE_CHECKED, kSessionButtonStyle, { 0 }, 0, -1 },
        { 10, ID_DESKTOP_VIEW_SESSION_TB,   TBSTATE_ENABLED,                   kSessionButtonStyle, { 0 }, 0, -1 },
        { 11, ID_FILE_TRANSFER_SESSION_TB,  TBSTATE_ENABLED,                   kSessionButtonStyle, { 0 }, 0, -1 },
        { 12, ID_SYSTEM_INFO_SESSION_TB,    TBSTATE_ENABLED,                   kSessionButtonStyle, { 0 }, 0, -1 },
        { 13, ID_POWER_MANAGE_SESSION_TB,   TBSTATE_ENABLED,                   kSessionButtonStyle, { 0 }, 0, -1 },
        { -1, 0,                            TBSTATE_ENABLED,                   BTNS_SEP,            { 0 }, 0, -1 },
        { 14, ID_ABOUT,                     TBSTATE_ENABLED,                   kButtonStyle,        { 0 }, 0, -1 },
        { 15, ID_EXIT,                      TBSTATE_ENABLED,                   kButtonStyle,        { 0 }, 0, -1 },
    };

    toolbar_.SetButtonStructSize(sizeof(kButtons[0]));
    toolbar_.AddButtons(_countof(kButtons), kButtons);

    if (toolbar_imagelist_.Create(small_icon_size.cx,
                                  small_icon_size.cy,
                                  ILC_MASK | ILC_COLOR32,
                                  1, 1))
    {
        toolbar_.SetImageList(toolbar_imagelist_);

        auto add_icon = [&](UINT icon_id)
        {
            CIcon icon(AtlLoadIconImage(icon_id,
                                        LR_CREATEDIBSECTION,
                                        small_icon_size.cx,
                                        small_icon_size.cy));
            toolbar_imagelist_.AddIcon(icon);
        };

        add_icon(IDI_DOCUMENT);
        add_icon(IDI_OPEN);
        add_icon(IDI_DISK);
        add_icon(IDI_FOLDER_ADD);
        add_icon(IDI_FOLDER_MINUS);
        add_icon(IDI_FOLDER_PENCIL);
        add_icon(IDI_COMPUTER_PLUS);
        add_icon(IDI_COMPUTER_MINUS);
        add_icon(IDI_COMPUTER_PENCIL);
        add_icon(IDI_MONITOR_WITH_KEYBOARD);
        add_icon(IDI_MONITOR);
        add_icon(IDI_FOLDER_STAND);
        add_icon(IDI_SYSTEM_MONITOR);
        add_icon(IDI_CONTROL_POWER);
        add_icon(IDI_ABOUT);
        add_icon(IDI_EXIT);
    }
}

void AddressBookWindow::InitComputerList(const CSize& small_icon_size)
{
    const DWORD style = WS_CHILD | WS_VISIBLE | LVS_REPORT | WS_TABSTOP | LVS_SHOWSELALWAYS;

    computer_list_ctrl_.Create(splitter_, rcDefault, nullptr, style,
                               WS_EX_CLIENTEDGE, kComputerListCtrl);

    DWORD ex_style = LVS_EX_FULLROWSELECT;

    if (IsWindowsVistaOrGreater())
    {
        ::SetWindowTheme(computer_list_ctrl_, L"explorer", nullptr);
        ex_style |= LVS_EX_DOUBLEBUFFER;
    }

    computer_list_ctrl_.SetExtendedListViewStyle(ex_style);

    computer_list_imagelist_.Create(small_icon_size.cx,
                                    small_icon_size.cy,
                                    ILC_MASK | ILC_COLOR32,
                                    1, 1);

    CIcon computer_icon = AtlLoadIconImage(IDI_COMPUTER,
                                           LR_CREATEDIBSECTION,
                                           small_icon_size.cx,
                                           small_icon_size.cy);
    computer_list_imagelist_.AddIcon(computer_icon);

    computer_list_ctrl_.SetImageList(computer_list_imagelist_, LVSIL_SMALL);

    auto add_column = [&](UINT resource_id, int column_index, int width)
    {
        CString text;
        text.LoadStringW(resource_id);

        computer_list_ctrl_.AddColumn(text, column_index);
        computer_list_ctrl_.SetColumnWidth(column_index, width);
    };

    add_column(IDS_COL_NAME, 0, 250);
    add_column(IDS_COL_ADDRESS, 1, 200);
    add_column(IDS_COL_PORT, 2, 100);
}

void AddressBookWindow::InitGroupTree(const CSize& small_icon_size)
{
    const DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | TVS_HASLINES |
        TVS_SHOWSELALWAYS | TVS_HASBUTTONS | TVS_LINESATROOT;

    group_tree_ctrl_.Create(splitter_, rcDefault, nullptr, style,
                            WS_EX_CLIENTEDGE, kGroupTreeCtrl);

    if (IsWindowsVistaOrGreater())
    {
        ::SetWindowTheme(group_tree_ctrl_, L"explorer", nullptr);
        static const DWORD kDoubleBuffer = 0x0004;
        group_tree_ctrl_.SetExtendedStyle(kDoubleBuffer, kDoubleBuffer);
    }

    group_tree_imagelist_.Create(small_icon_size.cx,
                                 small_icon_size.cy,
                                 ILC_MASK | ILC_COLOR32,
                                 1, 1);

    CIcon address_book_icon = AtlLoadIconImage(IDI_BOOKS_STACK,
                                               LR_CREATEDIBSECTION,
                                               small_icon_size.cx,
                                               small_icon_size.cy);

    CIcon group_icon = AtlLoadIconImage(IDI_FOLDER,
                                        LR_CREATEDIBSECTION,
                                        small_icon_size.cx,
                                        small_icon_size.cy);

    group_tree_imagelist_.AddIcon(address_book_icon);
    group_tree_imagelist_.AddIcon(group_icon);

    group_tree_ctrl_.SetImageList(group_tree_imagelist_);
}

void AddressBookWindow::SetAddressBookChanged(bool is_changed)
{
    address_book_changed_ = is_changed;

    CMenuHandle file_menu(main_menu_.GetSubMenu(0));

    if (address_book_changed_)
    {
        toolbar_.EnableButton(ID_SAVE, TRUE);
        file_menu.EnableMenuItem(ID_SAVE, MF_BYCOMMAND | MF_ENABLED);
    }
    else
    {
        toolbar_.EnableButton(ID_SAVE, FALSE);
        file_menu.EnableMenuItem(ID_SAVE, MF_BYCOMMAND | MF_DISABLED);
    }
}

void AddressBookWindow::AddChildComputerGroups(
    HTREEITEM parent_item, proto::ComputerGroup* parent_computer_group)
{
    for (int i = 0; i < parent_computer_group->group_size(); ++i)
    {
        proto::ComputerGroup* child_group = parent_computer_group->mutable_group(i);

        HTREEITEM item = group_tree_ctrl_.InsertItem(
            UNICODEfromUTF8(child_group->name()).c_str(),
            kComputerGroupIcon,
            kComputerGroupIcon,
            parent_item,
            TVI_LAST);

        // Each element in the tree contains a pointer to the computer group.
        group_tree_ctrl_.SetItemData(item, reinterpret_cast<DWORD_PTR>(child_group));

        AddChildComputerGroups(item, child_group);

        if (child_group->expanded())
            group_tree_ctrl_.Expand(item, TVE_EXPAND);
    }
}

void AddressBookWindow::AddChildComputers(proto::ComputerGroup* computer_group)
{
    for (int i = 0; i < computer_group->computer_size(); ++i)
    {
        proto::Computer* computer = computer_group->mutable_computer(i);

        int item_index = computer_list_ctrl_.AddItem(
            i, 0, UNICODEfromUTF8(computer->name()).c_str(), kComputerIcon);

        computer_list_ctrl_.SetItemData(item_index, reinterpret_cast<DWORD_PTR>(computer));

        computer_list_ctrl_.SetItemText(item_index, 1,
            UNICODEfromUTF8(computer->address()).c_str());

        computer_list_ctrl_.SetItemText(item_index, 2,
            std::to_wstring(computer->port()).c_str());
    }
}

bool AddressBookWindow::OpenAddressBook()
{
    if (!CloseAddressBook())
        return false;

    if (address_book_path_.empty())
    {
        WCHAR filter[256] = { 0 };
        int length = 0;

        length += AtlLoadString(IDS_ADDRESS_BOOK_FILTER, filter, ARRAYSIZE(filter)) + 1;
        StringCbCatW(filter + length, ARRAYSIZE(filter) - length, kAddressBookFileExtension);
        length += ARRAYSIZE(kAddressBookFileExtension);

        CFileDialog dialog(TRUE, kAddressBookFileExtension, L"", OFN_HIDEREADONLY, filter);
        if (dialog.DoModal() == IDCANCEL)
            return false;

        address_book_path_ = dialog.m_szFileName;
    }

    std::ifstream file;

    file.open(address_book_path_, std::ifstream::binary);
    if (!file.is_open())
    {
        CString message;
        message.LoadStringW(IDS_UNABLE_TO_OPEN_FILE_ERROR);

        MessageBoxW(message, nullptr, MB_OK | MB_ICONWARNING);
        return false;
    }

    file.seekg(0, file.end);
    std::streamsize file_size = file.tellg();
    file.seekg(0);

    std::string buffer;
    buffer.resize(static_cast<size_t>(file_size));

    file.read(buffer.data(), file_size);
    if (file.fail())
    {
        CString message;
        message.LoadStringW(IDS_UNABLE_TO_READ_FILE_ERROR);

        MessageBoxW(message, nullptr, MB_OK | MB_ICONWARNING);
        return false;
    }

    address_book_ = std::make_unique<proto::AddressBook>();

    if (!address_book_->ParseFromString(buffer))
    {
        address_book_.reset();

        CString message;
        message.LoadStringW(IDS_UNABLE_TO_READ_FILE_ERROR);

        MessageBoxW(message, nullptr, MB_OK | MB_ICONWARNING);
        return false;
    }

    HTREEITEM root_item = group_tree_ctrl_.InsertItem(
        UNICODEfromUTF8(address_book_->root_group().name()).c_str(),
        kAddressBookIcon,
        kAddressBookIcon,
        TVI_ROOT,
        TVI_LAST);

    AddChildComputerGroups(root_item, address_book_->mutable_root_group());
    AddChildComputers(address_book_->mutable_root_group());

    group_tree_ctrl_.Expand(root_item, TVE_EXPAND);

    group_tree_ctrl_.EnableWindow(TRUE);
    computer_list_ctrl_.EnableWindow(TRUE);

    CMenuHandle file_menu(main_menu_.GetSubMenu(0));
    file_menu.EnableMenuItem(ID_SAVE_AS, MF_BYCOMMAND | MF_ENABLED);
    file_menu.EnableMenuItem(ID_CLOSE, MF_BYCOMMAND | MF_ENABLED);

    return true;
}

bool AddressBookWindow::SaveAddressBook(const std::experimental::filesystem::path& path)
{
    if (!address_book_)
        return false;

    std::experimental::filesystem::path address_book_path(path);

    if (address_book_path.empty())
    {
        WCHAR filter[256] = { 0 };
        int length = 0;

        length += AtlLoadString(IDS_ADDRESS_BOOK_FILTER, filter, ARRAYSIZE(filter)) + 1;
        StringCbCatW(filter + length, ARRAYSIZE(filter) - length, kAddressBookFileExtension);
        length += ARRAYSIZE(kAddressBookFileExtension);

        CFileDialog dialog(FALSE, L"aad", L"", OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, filter);
        if (dialog.DoModal() == IDCANCEL)
            return false;

        address_book_path = dialog.m_szFileName;
    }

    std::ofstream file;

    file.open(address_book_path_, std::ofstream::binary | std::ofstream::trunc);
    if (!file.is_open())
    {
        CString message;
        message.LoadStringW(IDS_UNABLE_TO_OPEN_FILE_ERROR);

        MessageBoxW(message, nullptr, MB_OK | MB_ICONWARNING);
        return false;
    }

    std::string buffer = address_book_->SerializeAsString();

    file.write(buffer.c_str(), buffer.size());
    if (file.fail())
    {
        CString message;
        message.LoadStringW(IDS_UNABLE_TO_WRITE_FILE_ERROR);

        MessageBoxW(message, nullptr, MB_OK | MB_ICONWARNING);
        return false;
    }

    address_book_path_ = address_book_path;
    SetAddressBookChanged(false);
    return true;
}

bool AddressBookWindow::CloseAddressBook()
{
    if (address_book_)
    {
        if (address_book_changed_)
        {
            CString title;
            title.LoadStringW(IDS_CONFIRMATION);

            CString message;
            message.LoadStringW(IDS_ADDRESS_BOOK_CHANGED);

            switch (MessageBoxW(message, title, MB_YESNOCANCEL | MB_ICONQUESTION))
            {
                case IDYES:
                    if (!SaveAddressBook(address_book_path_))
                        return false;
                    break;

                case IDCANCEL:
                    return false;

                default:
                    break;
            }
        }

        address_book_path_.clear();
        address_book_.reset();
    }

    computer_list_ctrl_.DeleteAllItems();
    computer_list_ctrl_.EnableWindow(FALSE);

    group_tree_ctrl_.DeleteAllItems();
    group_tree_ctrl_.EnableWindow(FALSE);

    toolbar_.EnableButton(ID_ADD_GROUP, FALSE);
    toolbar_.EnableButton(ID_DELETE_GROUP, FALSE);
    toolbar_.EnableButton(ID_EDIT_GROUP, FALSE);
    toolbar_.EnableButton(ID_ADD_COMPUTER, FALSE);
    toolbar_.EnableButton(ID_DELETE_COMPUTER, FALSE);
    toolbar_.EnableButton(ID_EDIT_COMPUTER, FALSE);

    CMenuHandle file_menu(main_menu_.GetSubMenu(0));
    file_menu.EnableMenuItem(ID_SAVE_AS, MF_BYCOMMAND | MF_DISABLED);
    file_menu.EnableMenuItem(ID_CLOSE, MF_BYCOMMAND | MF_DISABLED);

    CMenuHandle edit_menu(main_menu_.GetSubMenu(1));
    edit_menu.EnableMenuItem(ID_ADD_GROUP, MF_BYCOMMAND | MF_DISABLED);
    edit_menu.EnableMenuItem(ID_DELETE_GROUP, MF_BYCOMMAND | MF_DISABLED);
    edit_menu.EnableMenuItem(ID_EDIT_GROUP, MF_BYCOMMAND | MF_DISABLED);
    edit_menu.EnableMenuItem(ID_ADD_COMPUTER, MF_BYCOMMAND | MF_DISABLED);
    edit_menu.EnableMenuItem(ID_DELETE_COMPUTER, MF_BYCOMMAND | MF_DISABLED);
    edit_menu.EnableMenuItem(ID_EDIT_COMPUTER, MF_BYCOMMAND | MF_DISABLED);

    SetAddressBookChanged(false);

    return true;
}

void AddressBookWindow::Connect(const proto::Computer& computer)
{
    if (!client_pool_)
        client_pool_ = std::make_unique<ClientPool>(MessageLoopProxy::Current());

    client_pool_->Connect(*this, computer);
}

} // namespace aspia
