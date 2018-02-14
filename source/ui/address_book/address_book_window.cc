//
// PROJECT:         Aspia
// FILE:            ui/address_book/address_book_window.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/address_book/address_book_window.h"

#include <functional>
#include <fstream>
#include <atldlgs.h>
#include <strsafe.h>

#include "base/strings/unicode.h"
#include "base/version_helpers.h"
#include "base/logging.h"
#include "crypto/sha.h"
#include "crypto/string_encryptor.h"
#include "network/url.h"
#include "ui/address_book/address_book_dialog.h"
#include "ui/address_book/address_book_secure_util.h"
#include "ui/address_book/computer_dialog.h"
#include "ui/address_book/computer_group_dialog.h"
#include "ui/address_book/open_address_book_dialog.h"
#include "ui/about_dialog.h"

namespace aspia {

namespace {

const WCHAR kAddressBookFileExtensionFilter[] = L"*.aab";
const WCHAR kAddressBookFileExtension[] = L"aab";

bool DeleteChildComputer(proto::ComputerGroup* parent_group, proto::Computer* computer)
{
    for (int i = 0; i < parent_group->computer_size(); ++i)
    {
        if (parent_group->mutable_computer(i) == computer)
        {
            SecureClearComputer(computer);
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
            SecureClearComputerGroup(child_group);
            parent_group->mutable_group()->DeleteSubrange(i, 1);
            return true;
        }
    }

    return false;
}

} // namespace

AddressBookWindow::AddressBookWindow(const std::experimental::filesystem::path& address_book_path)
    : address_book_path_(address_book_path)
{
    // Nothing
}

AddressBookWindow::~AddressBookWindow()
{
    SecureClearComputerGroup(&root_group_);
}

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
    title.LoadStringW(IDS_AB_TITLE);
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
                                         ID_SYSTEM_INFO_SESSION_TB,
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

    group_tree_ctrl_.EnableWindow(FALSE);
    computer_list_ctrl_.EnableWindow(FALSE);

    SetWindowPos(nullptr, 0, 0, 980, 700, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
    CenterWindow();

    if (!address_book_path_.empty())
        OpenAddressBook();

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
    if (state_ == State::OPENED_CHANGED)
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
    if (drag_source_ == DragSource::NONE)
        return 0;

    CPoint pos(lparam);
    ClientToScreen(&pos);

    drag_imagelist_.DragMove(pos.x + 15, pos.y + 15);
    drag_imagelist_.DragShowNolock(FALSE);

    group_tree_ctrl_.GetParent().ScreenToClient(&pos);

    HTREEITEM target_item = group_tree_ctrl_.HitTest(pos, 0);

    if (drag_source_ == DragSource::GROUP_TREE)
    {
        if (group_tree_ctrl_.IsAllowedDropTarget(target_item, group_tree_edited_item_))
            group_tree_ctrl_.SelectDropTarget(target_item);
    }
    else
    {
        DCHECK_EQ(drag_source_, DragSource::COMPUTER_LIST);
        group_tree_ctrl_.SelectDropTarget(target_item);
    }

    drag_imagelist_.DragShowNolock(TRUE);

    return 0;
}

bool AddressBookWindow::MoveGroup(HTREEITEM target_item, HTREEITEM source_item)
{
    HTREEITEM old_parent_item = group_tree_ctrl_.GetParentItem(source_item);
    if (!old_parent_item)
        return false;

    // The previous parent group matches the new (the position of the element has not changed).
    if (old_parent_item == target_item)
        return false;

    if (!group_tree_ctrl_.IsAllowedDropTarget(target_item, source_item))
        return false;

    proto::ComputerGroup* old_parent_group = group_tree_ctrl_.GetComputerGroup(old_parent_item);
    proto::ComputerGroup* new_parent_group = group_tree_ctrl_.GetComputerGroup(target_item);
    proto::ComputerGroup* old_group = group_tree_ctrl_.GetComputerGroup(source_item);

    if (!old_parent_group || !new_parent_group || !old_group)
        return false;

    // Create group in new place.
    proto::ComputerGroup* new_group = new_parent_group->add_group();

    // Move source group to destonation group.
    *new_group = std::move(*old_group);

    // Add group tree to UI control.
    HTREEITEM item = group_tree_ctrl_.AddComputerGroupTree(target_item, new_group);
    if (item)
    {
        group_tree_ctrl_.Expand(target_item, TVE_EXPAND);
        group_tree_ctrl_.SelectItem(item);

        // Now we can remove the group from the previous place.
        if (DeleteChildGroup(old_parent_group, old_group))
        {
            // Remove group from UI control.
            group_tree_ctrl_.DeleteItem(source_item);
        }
    }

    return true;
}

bool AddressBookWindow::MoveComputer(
    HTREEITEM target_item, proto::ComputerGroup* old_parent_group, int computer_index)
{
    if (!target_item || target_item == TVI_ROOT || !old_parent_group || computer_index == -1)
        return false;

    proto::ComputerGroup* new_parent_group = group_tree_ctrl_.GetComputerGroup(target_item);
    if (!new_parent_group)
        return false;

    proto::Computer* old_computer = computer_list_ctrl_.GetComputer(computer_index);
    if (!old_computer)
        return false;

    proto::Computer* new_computer = new_parent_group->add_computer();

    *new_computer = std::move(*old_computer);

    if (DeleteChildComputer(old_parent_group, old_computer))
    {
        computer_list_ctrl_.DeleteItem(computer_index);
    }

    return true;
}

LRESULT AddressBookWindow::OnLButtonUp(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    if (drag_source_ == DragSource::NONE)
        return 0;

    DragSource source = drag_source_;
    drag_source_ = DragSource::NONE;

    if (source == DragSource::GROUP_TREE)
    {
        drag_imagelist_.DragLeave(group_tree_ctrl_);
    }
    else
    {
        DCHECK_EQ(source, DragSource::COMPUTER_LIST);
        drag_imagelist_.DragLeave(computer_list_ctrl_);
    }

    drag_imagelist_.EndDrag();
    drag_imagelist_.Destroy();

    CPoint cursor_pos;
    GetCursorPos(&cursor_pos);

    group_tree_ctrl_.GetParent().ScreenToClient(&cursor_pos);

    HTREEITEM target_item = group_tree_ctrl_.HitTest(cursor_pos, 0);

    group_tree_ctrl_.SelectDropTarget(nullptr);

    if (source == DragSource::GROUP_TREE)
    {
        if (MoveGroup(target_item, group_tree_edited_item_))
        {
            // Mark the address book as changed.
            SetAddressBookChanged(true);
        }
    }
    else
    {
        DCHECK_EQ(source, DragSource::COMPUTER_LIST);

        if (MoveComputer(target_item, drag_computer_group_, drag_computer_index_))
        {
            // Mark the address book as changed.
            SetAddressBookChanged(true);
        }
    }

    ReleaseCapture();

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

    switch (computer_list_ctrl_.GetSelectedCount())
    {
        case 0:
            menu = AtlLoadMenu(IDR_COMPUTER_LIST);
            break;

        case 1:
            menu = AtlLoadMenu(IDR_COMPUTER_LIST_ITEM);
            break;

        default:
            return 0;
    }

    POINT cursor_pos;
    GetCursorPos(&cursor_pos);

    SetForegroundWindow(*this);

    CMenuHandle popup_menu(menu.GetSubMenu(0));
    popup_menu.TrackPopupMenu(0, cursor_pos.x, cursor_pos.y, *this, nullptr);

    return 0;
}

LRESULT AddressBookWindow::OnComputerListColumnClick(
    int /* control_id */, LPNMHDR hdr, BOOL& /* handled */)
{
    LPNMLISTVIEW pnmv = reinterpret_cast<LPNMLISTVIEW>(hdr);
    computer_list_ctrl_.SortItemsByColumn(pnmv->iSubItem);
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

LRESULT AddressBookWindow::OnComputerListBeginDrag(
    int /* control_id */, LPNMHDR hdr, BOOL& /* handled */)
{
    drag_computer_index_ = computer_list_ctrl_.GetNextItem(-1, LVNI_SELECTED);
    if (drag_computer_index_ == -1)
        return 0;

    HTREEITEM parent_item = group_tree_ctrl_.GetSelectedItem();
    if (!parent_item)
        return 0;

    drag_computer_group_ = group_tree_ctrl_.GetComputerGroup(parent_item);
    if (!drag_computer_group_)
        return 0;

    CPoint p(0, 0);

    drag_imagelist_ = computer_list_ctrl_.CreateDragImage(drag_computer_index_, &p);
    if (drag_imagelist_.IsNull())
        return 0;

    CPoint action_point = reinterpret_cast<LPNMLISTVIEW>(hdr)->ptAction;

    computer_list_ctrl_.ClientToScreen(&action_point);

    drag_imagelist_.BeginDrag(0, 0, 0);
    drag_imagelist_.DragEnter(GetDesktopWindow(), action_point);
    SetCapture();

    drag_source_ = DragSource::COMPUTER_LIST;
    return 0;
}

LRESULT AddressBookWindow::OnGroupSelected(int /* control_id */, LPNMHDR hdr, BOOL& /* handled */)
{
    LPNMTREEVIEWW header = reinterpret_cast<LPNMTREEVIEWW>(hdr);

    group_tree_edited_item_ = header->itemNew.hItem;

    CMenuHandle edit_menu(main_menu_.GetSubMenu(1));

    toolbar_.EnableButton(ID_ADD_GROUP, TRUE);
    toolbar_.EnableButton(ID_EDIT_GROUP, TRUE);
    toolbar_.EnableButton(ID_ADD_COMPUTER, TRUE);
    toolbar_.EnableButton(ID_DELETE_COMPUTER, FALSE);
    toolbar_.EnableButton(ID_EDIT_COMPUTER, FALSE);

    edit_menu.EnableMenuItem(ID_ADD_GROUP, MF_BYCOMMAND | MF_ENABLED);
    edit_menu.EnableMenuItem(ID_ADD_COMPUTER, MF_BYCOMMAND | MF_ENABLED);
    edit_menu.EnableMenuItem(ID_DELETE_COMPUTER, MF_BYCOMMAND | MF_DISABLED);
    edit_menu.EnableMenuItem(ID_EDIT_COMPUTER, MF_BYCOMMAND | MF_DISABLED);

    if (group_tree_ctrl_.GetFirstVisibleItem() == header->itemNew.hItem)
    {
        toolbar_.EnableButton(ID_DELETE_GROUP, FALSE);

        edit_menu.EnableMenuItem(ID_EDIT_GROUP, MF_BYCOMMAND | MF_DISABLED);
        edit_menu.EnableMenuItem(ID_DELETE_GROUP, MF_BYCOMMAND | MF_DISABLED);
    }
    else
    {
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

    group_tree_edited_item_ = group_tree_ctrl_.HitTest(cursor_pos, 0);
    if (!group_tree_edited_item_)
        return 0;

    group_tree_ctrl_.GetParent().ClientToScreen(&cursor_pos);
    SetForegroundWindow(*this);

    CMenu menu = AtlLoadMenu(IDR_GROUP_TREE_ITEM);
    CMenuHandle popup_menu(menu.GetSubMenu(0));

    if (group_tree_edited_item_ == group_tree_ctrl_.GetFirstVisibleItem())
        popup_menu.EnableMenuItem(ID_DELETE_GROUP, MF_BYCOMMAND | MF_DISABLED);

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

    drag_imagelist_ = group_tree_ctrl_.CreateDragImage(pnmtv->itemNew.hItem);
    if (drag_imagelist_.IsNull())
        return 0;

    drag_imagelist_.BeginDrag(0, 0, 0);
    drag_imagelist_.DragEnter(GetDesktopWindow(), pnmtv->ptDrag.x, pnmtv->ptDrag.y);
    SetCapture();

    group_tree_edited_item_ = pnmtv->itemNew.hItem;
    drag_source_ = DragSource::GROUP_TREE;
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
    SaveAddressBook(address_book_path_);
    return 0;
}

LRESULT AddressBookWindow::OnSaveAsButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    SaveAddressBook(std::experimental::filesystem::path());
    return 0;
}

LRESULT AddressBookWindow::OnHelpButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    CString url;
    url.LoadStringW(IDS_HELP_LINK);
    URL::FromCString(url).Open();
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

    AddressBookDialog dialog;
    if (dialog.DoModal() == IDCANCEL)
        return 0;

    root_group_.CopyFrom(dialog.GetRootComputerGroup());
    encryption_type_ = dialog.GetEncryptionType();

    key_.reset(dialog.GetKey());

    computer_list_ctrl_.EnableWindow(TRUE);
    group_tree_ctrl_.EnableWindow(TRUE);
    group_tree_ctrl_.AddComputerGroupTree(TVI_ROOT, &root_group_);

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
        SecureClearComputer(computer);

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

    if (group_tree_edited_item_ == group_tree_ctrl_.GetFirstVisibleItem())
    {
        AddressBookDialog dialog(encryption_type_, root_group_, key_.string());

        if (dialog.DoModal() == IDOK)
        {
            SecureClearComputerGroup(selected_group);

            encryption_type_ = dialog.GetEncryptionType();
            key_.reset(dialog.GetKey());

            selected_group->CopyFrom(dialog.GetRootComputerGroup());
            group_tree_ctrl_.UpdateComputerGroup(group_tree_edited_item_, selected_group);
            SetAddressBookChanged(true);
        }
    }
    else
    {
        ComputerGroupDialog dialog(*selected_group);
        if (dialog.DoModal() == IDOK)
        {
            SecureClearComputerGroup(selected_group);

            selected_group->CopyFrom(dialog.GetComputerGroup());
            group_tree_ctrl_.UpdateComputerGroup(group_tree_edited_item_, selected_group);
            SetAddressBookChanged(true);
        }
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
    message.Format(IDS_AB_COMPUTER_DELETE_CONFIRMATION,
                   UNICODEfromUTF8(computer->name()).c_str());

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
    message.Format(IDS_AB_GROUP_DELETE_CONFIRMATION,
                   UNICODEfromUTF8(selected_group->name()).c_str());

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
                                         ID_SYSTEM_INFO_SESSION_TB,
                                         control_id,
                                         MF_BYCOMMAND);
    toolbar_.CheckButton(control_id, TRUE);
    return 0;
}

void AddressBookWindow::SetAddressBookChanged(bool is_changed)
{
    state_ = is_changed ? State::OPENED_CHANGED : State::OPENED_NOT_CHANGED;

    CMenuHandle file_menu(main_menu_.GetSubMenu(0));

    if (state_ == State::OPENED_CHANGED)
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

        length += AtlLoadString(IDS_AB_ADDRESS_BOOK_FILTER, filter, ARRAYSIZE(filter)) + 1;
        StringCbCatW(filter + length, ARRAYSIZE(filter) - length, kAddressBookFileExtensionFilter);
        length += ARRAYSIZE(kAddressBookFileExtensionFilter);

        CFileDialog dialog(TRUE, kAddressBookFileExtension, L"", OFN_HIDEREADONLY, filter);
        if (dialog.DoModal() == IDCANCEL)
            return false;

        address_book_path_ = dialog.m_szFileName;
    }

    std::ifstream file;

    file.open(address_book_path_, std::ifstream::binary);
    if (!file.is_open())
    {
        ShowErrorMessage(IDS_AB_UNABLE_TO_OPEN_FILE_ERROR);
        return false;
    }

    file.seekg(0, file.end);
    std::streamsize file_size = file.tellg();
    file.seekg(0);

    SecureString<std::string> buffer;
    buffer.mutable_string().resize(static_cast<size_t>(file_size));

    file.read(buffer.mutable_string().data(), file_size);
    if (file.fail())
    {
        ShowErrorMessage(IDS_AB_UNABLE_TO_READ_FILE_ERROR);
        return false;
    }

    proto::AddressBook address_book;

    if (!address_book.ParseFromString(buffer.string()))
    {
        ShowErrorMessage(IDS_AB_CORRUPTED_FILE_ERROR);
        return false;
    }

    encryption_type_ = address_book.encryption_type();

    switch (encryption_type_)
    {
        case proto::AddressBook::ENCRYPTION_TYPE_NONE:
        {
            if (!root_group_.ParseFromString(address_book.data()))
            {
                ShowErrorMessage(IDS_AB_CORRUPTED_FILE_ERROR);
                return false;
            }
        }
        break;

        case proto::AddressBook::ENCRYPTION_TYPE_XCHACHA20_POLY1305:
        {
            OpenAddressBookDialog dialog;

            if (dialog.DoModal() == IDCANCEL)
                return false;

            key_ = SHA256(dialog.GetPassword(), 1000);

            SecureString<std::string> decrypted;

            if (!DecryptString(address_book.data(), key_.string(), decrypted.mutable_string()))
            {
                ShowErrorMessage(IDS_AB_UNABLE_TO_DECRYPT_ERROR);
                return false;
            }

            if (!root_group_.ParseFromString(decrypted.string()))
            {
                ShowErrorMessage(IDS_AB_CORRUPTED_FILE_ERROR);
                return false;
            }
        }
        break;

        default:
        {
            LOG(LS_WARNING) << "An attempt to open an address book with an unknown"
                            << " type of encryption: " << encryption_type_;
            ShowErrorMessage(IDS_AB_UNKNOWN_ENCRYPTION_TYPE_ERROR);
            return false;
        }
    }

    HTREEITEM root_item = group_tree_ctrl_.AddComputerGroupTree(TVI_ROOT, &root_group_);
    group_tree_ctrl_.Expand(root_item, TVE_EXPAND);

    computer_list_ctrl_.AddChildComputers(&root_group_);

    group_tree_ctrl_.EnableWindow(TRUE);
    computer_list_ctrl_.EnableWindow(TRUE);

    CMenuHandle file_menu(main_menu_.GetSubMenu(0));
    file_menu.EnableMenuItem(ID_SAVE_AS, MF_BYCOMMAND | MF_ENABLED);
    file_menu.EnableMenuItem(ID_CLOSE, MF_BYCOMMAND | MF_ENABLED);

    group_tree_ctrl_.SelectItem(root_item);
    group_tree_ctrl_.SetFocus();

    return true;
}

bool AddressBookWindow::SaveAddressBook(const std::experimental::filesystem::path& path)
{
    if (state_ == State::CLOSED)
        return false;

    std::experimental::filesystem::path address_book_path(path);

    if (address_book_path.empty())
    {
        WCHAR filter[256] = { 0 };
        int length = 0;

        length += AtlLoadString(IDS_AB_ADDRESS_BOOK_FILTER, filter, ARRAYSIZE(filter)) + 1;
        StringCbCatW(filter + length, ARRAYSIZE(filter) - length, kAddressBookFileExtensionFilter);
        length += ARRAYSIZE(kAddressBookFileExtensionFilter);

        CFileDialog dialog(FALSE, kAddressBookFileExtension, L"",
                           OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, filter);
        if (dialog.DoModal() == IDCANCEL)
            return false;

        address_book_path = dialog.m_szFileName;
    }

    std::ofstream file;

    file.open(address_book_path, std::ofstream::binary | std::ofstream::trunc);
    if (!file.is_open())
    {
        ShowErrorMessage(IDS_AB_UNABLE_TO_OPEN_FILE_ERROR);
        return false;
    }

    proto::AddressBook address_book;
    address_book.set_encryption_type(encryption_type_);

    SecureString<std::string> serialized(root_group_.SerializeAsString());

    switch (encryption_type_)
    {
        case proto::AddressBook::ENCRYPTION_TYPE_NONE:
        {
            address_book.set_data(serialized.string());
        }
        break;

        case proto::AddressBook::ENCRYPTION_TYPE_XCHACHA20_POLY1305:
        {
            address_book.set_data(EncryptString(serialized.string(), key_.string()));
        }
        break;

        default:
            DLOG(LS_FATAL) << "Unexpected encryption type: " << encryption_type_;
            break;
    }

    SecureString<std::string> buffer = address_book.SerializeAsString();

    file.write(buffer.string().c_str(), buffer.string().size());
    if (file.fail())
    {
        ShowErrorMessage(IDS_AB_UNABLE_TO_WRITE_FILE_ERROR);
        return false;
    }

    address_book_path_ = address_book_path;
    SetAddressBookChanged(false);
    return true;
}

bool AddressBookWindow::CloseAddressBook()
{
    if (state_ != State::CLOSED)
    {
        if (state_ == State::OPENED_CHANGED)
        {
            CString title;
            title.LoadStringW(IDS_CONFIRMATION);

            CString message;
            message.LoadStringW(IDS_AB_ADDRESS_BOOK_CHANGED);

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
        state_ = State::CLOSED;
    }

    SecureClearComputerGroup(&root_group_);
    key_.reset();

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

void AddressBookWindow::ShowErrorMessage(UINT string_id)
{
    CString message;
    message.LoadStringW(string_id);
    MessageBoxW(message, nullptr, MB_OK | MB_ICONWARNING);
}

} // namespace aspia
