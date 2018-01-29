//
// PROJECT:         Aspia
// FILE:            ui/address_book_window.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/address_book_window.h"

#include <atldlgs.h>
#include <strsafe.h>
#include <functional>
#include <fstream>

#include "base/strings/unicode.h"
#include "base/version_helpers.h"
#include "ui/about_dialog.h"
#include "ui/computer_dialog.h"
#include "ui/computer_group_dialog.h"

namespace aspia {

namespace {

const WCHAR kAddressBookFileExtension[] = L"*.aad";

bool DeleteChildComputer(proto::ComputerGroup* parent_group, proto::Computer* computer)
{
    for (int i = 0; i < parent_group->computer_size(); ++i)
    {
        if (parent_group->mutable_computer(i) == computer)
        {
            parent_group->mutable_computer()->DeleteSubrange(i, 1);
            return true;
        }
    }

    return false;
}

bool DeleteChildGroup(proto::ComputerGroup* parent_group, proto::ComputerGroup* child_group)
{
    for (int i = 0; i < parent_group->group_size(); ++i)
    {
        if (parent_group->mutable_group(i) == child_group)
        {
            parent_group->mutable_group()->DeleteSubrange(i, 1);
            return true;
        }
    }

    return false;
}

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

    group_tree_ctrl_.Create(splitter_, kGroupTreeCtrl);
    computer_list_ctrl_.Create(splitter_, kComputerListCtrl);

    splitter_.SetSplitterPane(SPLIT_PANE_LEFT, group_tree_ctrl_);
    splitter_.SetSplitterPane(SPLIT_PANE_RIGHT, computer_list_ctrl_);

    toolbar_.Create(*this);

    if (!address_book_)
    {
        group_tree_ctrl_.EnableWindow(FALSE);
        computer_list_ctrl_.EnableWindow(FALSE);
    }

    SetWindowPos(nullptr, 0, 0, 980, 700, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
    CenterWindow();

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
        if (!CloseAddressBook())
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

LRESULT AddressBookWindow::OnMouseMove(
    UINT /* message */, WPARAM /* wparam */, LPARAM lparam, BOOL& /* handled */)
{
    CPoint pos(lparam);

    if (group_tree_dragging_)
    {
        ClientToScreen(&pos);

        group_tree_drag_imagelist_.DragMove(pos.x + 15, pos.y + 15);
        group_tree_drag_imagelist_.DragShowNolock(FALSE);

        group_tree_ctrl_.GetParent().ScreenToClient(&pos);

        TVHITTESTINFO hit_test_info;
        hit_test_info.pt.x = pos.x;
        hit_test_info.pt.y = pos.y;

        HTREEITEM target_item = group_tree_ctrl_.HitTest(&hit_test_info);

        if (target_item &&
            // A moved element can not be moved to a child element.
            !group_tree_ctrl_.IsItemContainsChild(group_tree_edited_item_, target_item))
        {
            group_tree_ctrl_.SendMessageW(TVM_SELECTITEM,
                                          TVGN_DROPHILITE,
                                          reinterpret_cast<LPARAM>(target_item));
        }

        group_tree_drag_imagelist_.DragShowNolock(TRUE);
    }

    return 0;
}

bool AddressBookWindow::MoveGroup(HTREEITEM target_item, HTREEITEM source_item)
{
    HTREEITEM old_parent_item = group_tree_ctrl_.GetParentItem(source_item);
    if (!old_parent_item || old_parent_item == target_item)
        return false;

    if (group_tree_ctrl_.IsItemContainsChild(source_item, target_item))
        return false;

    proto::ComputerGroup* old_parent_group = group_tree_ctrl_.GetComputerGroup(old_parent_item);
    proto::ComputerGroup* new_parent_group = group_tree_ctrl_.GetComputerGroup(target_item);
    proto::ComputerGroup* group_to_move = group_tree_ctrl_.GetComputerGroup(source_item);

    if (!old_parent_group || !new_parent_group || !group_to_move)
        return false;

    proto::ComputerGroup temp(*group_to_move);

    if (DeleteChildGroup(old_parent_group, group_to_move))
    {
        group_tree_ctrl_.DeleteItem(source_item);

        proto::ComputerGroup* group = new_parent_group->add_group();
        group->CopyFrom(temp);

        HTREEITEM item = group_tree_ctrl_.AddComputerGroupTree(target_item, group);

        group_tree_ctrl_.Expand(target_item, TVE_EXPAND);
        group_tree_ctrl_.SelectItem(item);

        SetAddressBookChanged(true);
    }

    return true;
}

LRESULT AddressBookWindow::OnLButtonUp(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    if (group_tree_dragging_)
    {
        group_tree_dragging_ = false;

        group_tree_drag_imagelist_.DragLeave(group_tree_ctrl_);
        group_tree_drag_imagelist_.EndDrag();
        group_tree_drag_imagelist_.Destroy();

        POINT cursor_pos;
        GetCursorPos(&cursor_pos);

        group_tree_ctrl_.GetParent().ScreenToClient(&cursor_pos);

        TVHITTESTINFO hit_test_info;
        hit_test_info.pt = cursor_pos;

        HTREEITEM target_item = group_tree_ctrl_.HitTest(&hit_test_info);

        group_tree_ctrl_.SendMessageW(TVM_SELECTITEM, TVGN_DROPHILITE, 0);

        if (target_item && target_item != TVI_ROOT)
            MoveGroup(target_item, group_tree_edited_item_);

        ReleaseCapture();
    }

    return 0;
}

LRESULT AddressBookWindow::OnComputerListDoubleClick(
    int /*control_id */, LPNMHDR /* hdr */, BOOL& /* handled */)
{
    int item_index = computer_list_ctrl_.GetNextItem(-1, LVNI_SELECTED);
    if (item_index == -1)
        return 0;

    proto::Computer* computer = computer_list_ctrl_.GetComputer(item_index);
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

    CMenuHandle edit_menu(main_menu_.GetSubMenu(1));

    toolbar_.EnableButton(ID_ADD_GROUP, TRUE);
    toolbar_.EnableButton(ID_ADD_COMPUTER, TRUE);

    edit_menu.EnableMenuItem(ID_ADD_GROUP, MF_BYCOMMAND | MF_ENABLED);
    edit_menu.EnableMenuItem(ID_ADD_COMPUTER, MF_BYCOMMAND | MF_ENABLED);

    if (group_tree_ctrl_.GetFirstVisibleItem() == header->itemNew.hItem)
    {
        toolbar_.EnableButton(ID_EDIT_GROUP, FALSE);
        toolbar_.EnableButton(ID_DELETE_GROUP, FALSE);

        edit_menu.EnableMenuItem(ID_EDIT_GROUP, MF_BYCOMMAND | MF_DISABLED);
        edit_menu.EnableMenuItem(ID_DELETE_GROUP, MF_BYCOMMAND | MF_DISABLED);
    }
    else
    {
        toolbar_.EnableButton(ID_EDIT_GROUP, TRUE);
        toolbar_.EnableButton(ID_DELETE_GROUP, TRUE);

        edit_menu.EnableMenuItem(ID_EDIT_GROUP, MF_BYCOMMAND | MF_ENABLED);
        edit_menu.EnableMenuItem(ID_DELETE_GROUP, MF_BYCOMMAND | MF_ENABLED);
    }

    computer_list_ctrl_.DeleteAllItems();

    proto::ComputerGroup* group =
        reinterpret_cast<proto::ComputerGroup*>(header->itemNew.lParam);
    if (group)
        computer_list_ctrl_.AddChildComputers(group);

    return 0;
}

LRESULT AddressBookWindow::OnGroupTreeRightClick(
    int /* control_id */, LPNMHDR /* hdr */, BOOL& /* handled */)
{
    POINT cursor_pos;
    GetCursorPos(&cursor_pos);

    group_tree_ctrl_.GetParent().ScreenToClient(&cursor_pos);

    TVHITTESTINFO hit_test_info;
    hit_test_info.pt = cursor_pos;

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

LRESULT AddressBookWindow::OnGroupTreeBeginDrag(
    int /* control_id */, LPNMHDR hdr, BOOL& /* handled */)
{
    LPNMTREEVIEWW pnmtv = reinterpret_cast<LPNMTREEVIEWW>(hdr);

    group_tree_drag_imagelist_ = group_tree_ctrl_.CreateDragImage(pnmtv->itemNew.hItem);
    if (group_tree_drag_imagelist_.IsNull())
        return 0;

    group_tree_drag_imagelist_.BeginDrag(0, 0, 0);
    group_tree_drag_imagelist_.DragEnter(GetDesktopWindow(), pnmtv->ptDrag.x, pnmtv->ptDrag.y);
    SetCapture();

    group_tree_edited_item_ = pnmtv->itemNew.hItem;
    group_tree_dragging_ = true;
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

    address_book_ = std::make_unique<proto::AddressBook>();
    address_book_->mutable_root_group()->set_name(UTF8fromUNICODE(name));

    group_tree_ctrl_.AddComputerGroupTree(TVI_ROOT, address_book_->mutable_root_group());

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
            group_tree_ctrl_.GetComputerGroup(group_tree_edited_item_);
        if (!parent_group)
            return 0;

        proto::Computer* computer = parent_group->add_computer();
        computer->CopyFrom(dialog.GetComputer());

        if (group_tree_edited_item_ != group_tree_ctrl_.GetSelectedItem())
        {
            group_tree_ctrl_.SelectItem(group_tree_edited_item_);
        }
        else
        {
            computer_list_ctrl_.AddComputer(computer);
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
            group_tree_ctrl_.GetComputerGroup(group_tree_edited_item_);
        if (!parent_group)
            return 0;

        proto::ComputerGroup* new_group = parent_group->add_group();
        new_group->CopyFrom(dialog.GetComputerGroup());

        HTREEITEM item = group_tree_ctrl_.AddComputerGroup(group_tree_edited_item_, new_group);

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

    proto::Computer* computer = computer_list_ctrl_.GetComputer(item_index);
    if (!computer)
        return 0;

    ComputerDialog dialog(*computer);
    if (dialog.DoModal() == IDOK)
    {
        computer->CopyFrom(dialog.GetComputer());
        computer_list_ctrl_.UpdateComputer(item_index, computer);
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
        group_tree_ctrl_.GetComputerGroup(group_tree_edited_item_);
    if (!selected_group)
        return 0;

    ComputerGroupDialog dialog(*selected_group);
    if (dialog.DoModal() == IDOK)
    {
        selected_group->CopyFrom(dialog.GetComputerGroup());
        group_tree_ctrl_.UpdateComputerGroup(group_tree_edited_item_, selected_group);
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

    proto::Computer* computer = computer_list_ctrl_.GetComputer(item_index);
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

        proto::ComputerGroup* parent_group = group_tree_ctrl_.GetComputerGroup(parent_group_item);
        if (!parent_group)
            return 0;

        if (DeleteChildComputer(parent_group, computer))
        {
            computer_list_ctrl_.DeleteItem(item_index);
            SetAddressBookChanged(true);
        }
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
        group_tree_ctrl_.GetComputerGroup(group_tree_edited_item_);
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

        proto::ComputerGroup* parent_group = group_tree_ctrl_.GetComputerGroup(parent_item);
        if (!parent_group)
            return 0;

        if (DeleteChildGroup(parent_group, selected_group))
        {
            group_tree_ctrl_.DeleteItem(group_tree_edited_item_);
            SetAddressBookChanged(true);
        }
    }

    return 0;
}

LRESULT AddressBookWindow::OnOpenSessionButton(
    WORD /* notify_code */, WORD control_id, HWND /* control */, BOOL& /* handled */)
{
    int item_index = computer_list_ctrl_.GetNextItem(-1, LVNI_SELECTED);
    if (item_index == -1)
        return 0;

    proto::Computer* computer = computer_list_ctrl_.GetComputer(item_index);
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

    HTREEITEM root_item = group_tree_ctrl_.AddComputerGroupTree(
        TVI_ROOT, address_book_->mutable_root_group());

    computer_list_ctrl_.AddChildComputers(address_book_->mutable_root_group());

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

    file.open(address_book_path, std::ofstream::binary | std::ofstream::trunc);
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
