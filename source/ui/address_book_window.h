//
// PROJECT:         Aspia
// FILE:            ui/address_book_window.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__ADDRESS_BOOK_WINDOW_H
#define _ASPIA_UI__ADDRESS_BOOK_WINDOW_H

#include "base/message_loop/message_loop.h"
#include "client/client_pool.h"
#include "proto/address_book.pb.h"
#include "ui/base/splitter.h"
#include "ui/address_book_toolbar.h"
#include "ui/computer_group_tree_ctrl.h"
#include "ui/computer_list_ctrl.h"
#include "ui/resource.h"

#include <experimental/filesystem>

namespace aspia {

class AddressBookWindow
    : public CWindowImpl<AddressBookWindow, CWindow, CFrameWinTraits>,
      public MessageLoop::Dispatcher
{
public:
    AddressBookWindow() = default;
    ~AddressBookWindow() = default;

private:
    // MessageLoop::Dispatcher implementation.
    bool Dispatch(const NativeEvent& event) override;

    static const int kGroupTreeCtrl = 100;
    static const int kComputerListCtrl = 101;

    BEGIN_MSG_MAP(AddressBookWindow)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        MESSAGE_HANDLER(WM_GETMINMAXINFO, OnGetMinMaxInfo)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
        MESSAGE_HANDLER(WM_LBUTTONUP, OnLButtonUp)

        NOTIFY_HANDLER(kComputerListCtrl, NM_DBLCLK, OnComputerListDoubleClick)
        NOTIFY_HANDLER(kComputerListCtrl, NM_RCLICK, OnComputerListRightClick)
        NOTIFY_HANDLER(kComputerListCtrl, LVN_ITEMCHANGED, OnComputerListItemChanged)
        NOTIFY_HANDLER(kGroupTreeCtrl, TVN_SELCHANGED, OnGroupSelected)
        NOTIFY_HANDLER(kGroupTreeCtrl, NM_RCLICK, OnGroupTreeRightClick)
        NOTIFY_HANDLER(kGroupTreeCtrl, TVN_ITEMEXPANDED, OnGroupTreeItemExpanded)
        NOTIFY_HANDLER(kGroupTreeCtrl, TVN_BEGINDRAG, OnGroupTreeBeginDrag)

        COMMAND_ID_HANDLER(ID_OPEN, OnOpenButton)
        COMMAND_ID_HANDLER(ID_SAVE, OnSaveButton)
        COMMAND_ID_HANDLER(ID_SAVE_AS, OnSaveAsButton)
        COMMAND_ID_HANDLER(ID_ABOUT, OnAboutButton)
        COMMAND_ID_HANDLER(ID_EXIT, OnExitButton)
        COMMAND_ID_HANDLER(ID_NEW, OnNewButton)
        COMMAND_ID_HANDLER(ID_CLOSE, OnCloseButton)
        COMMAND_ID_HANDLER(ID_ADD_COMPUTER, OnAddComputerButton)
        COMMAND_ID_HANDLER(ID_ADD_GROUP, OnAddGroupButton)
        COMMAND_ID_HANDLER(ID_EDIT_COMPUTER, OnEditComputerButton)
        COMMAND_ID_HANDLER(ID_EDIT_GROUP, OnEditGroupButton)
        COMMAND_ID_HANDLER(ID_DELETE_COMPUTER, OnDeleteComputerButton)
        COMMAND_ID_HANDLER(ID_DELETE_GROUP, OnDeleteGroupButton)

        // Session select from popup menu.
        COMMAND_ID_HANDLER(ID_DESKTOP_MANAGE_SESSION, OnOpenSessionButton)
        COMMAND_ID_HANDLER(ID_DESKTOP_VIEW_SESSION, OnOpenSessionButton)
        COMMAND_ID_HANDLER(ID_FILE_TRANSFER_SESSION, OnOpenSessionButton)
        COMMAND_ID_HANDLER(ID_SYSTEM_INFO_SESSION, OnOpenSessionButton)
        COMMAND_ID_HANDLER(ID_POWER_MANAGE_SESSION, OnOpenSessionButton)

        // Session select from main menu or toolbar.
        COMMAND_ID_HANDLER(ID_DESKTOP_MANAGE_SESSION_TB, OnSelectSessionButton)
        COMMAND_ID_HANDLER(ID_DESKTOP_VIEW_SESSION_TB, OnSelectSessionButton)
        COMMAND_ID_HANDLER(ID_FILE_TRANSFER_SESSION_TB, OnSelectSessionButton)
        COMMAND_ID_HANDLER(ID_SYSTEM_INFO_SESSION_TB, OnSelectSessionButton)
        COMMAND_ID_HANDLER(ID_POWER_MANAGE_SESSION_TB, OnSelectSessionButton)
    END_MSG_MAP()

    LRESULT OnCreate(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnDestroy(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnSize(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnGetMinMaxInfo(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnMouseMove(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnLButtonUp(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);

    LRESULT OnComputerListDoubleClick(int control_id, LPNMHDR hdr, BOOL& handled);
    LRESULT OnComputerListRightClick(int control_id, LPNMHDR hdr, BOOL& handled);
    LRESULT OnComputerListItemChanged(int control_id, LPNMHDR hdr, BOOL& handled);
    LRESULT OnGroupSelected(int control_id, LPNMHDR hdr, BOOL& handled);
    LRESULT OnGroupTreeRightClick(int control_id, LPNMHDR hdr, BOOL& handled);
    LRESULT OnGroupTreeItemExpanded(int control_id, LPNMHDR hdr, BOOL& handled);
    LRESULT OnGroupTreeBeginDrag(int control_id, LPNMHDR hdr, BOOL& handled);

    LRESULT OnOpenButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnSaveButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnSaveAsButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnAboutButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnExitButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnNewButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnCloseButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnAddComputerButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnAddGroupButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnEditComputerButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnEditGroupButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnDeleteComputerButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnDeleteGroupButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnOpenSessionButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnSelectSessionButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);

    bool MoveGroup(HTREEITEM target_item, HTREEITEM source_item);
    void SetAddressBookChanged(bool is_changed);
    bool OpenAddressBook();
    bool SaveAddressBook(const std::experimental::filesystem::path& path);
    bool CloseAddressBook();
    void Connect(const proto::Computer& computer);

    CIcon small_icon_;
    CIcon big_icon_;

    CAccelerator accelerator_;
    CMenu main_menu_;

    VerticalSplitter splitter_;
    CStatusBarCtrl statusbar_;

    ComputerGroupTreeCtrl group_tree_ctrl_;
    CImageListManaged group_tree_drag_imagelist_;
    bool group_tree_dragging_ = false;
    HTREEITEM group_tree_edited_item_ = nullptr;

    ComputerListCtrl computer_list_ctrl_;

    AddressBookToolbar toolbar_;

    std::unique_ptr<proto::AddressBook> address_book_;
    std::experimental::filesystem::path address_book_path_;
    bool address_book_changed_ = false;

    std::unique_ptr<ClientPool> client_pool_;
};

} // namespace aspia

#endif // _ASPIA_UI__ADDRESS_BOOK_WINDOW_H
