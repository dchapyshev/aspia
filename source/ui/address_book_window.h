//
// PROJECT:         Aspia
// FILE:            ui/address_book_window.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__ADDRESS_BOOK_WINDOW_H
#define _ASPIA_UI__ADDRESS_BOOK_WINDOW_H

#include "base/message_loop/message_loop.h"
#include "ui/base/splitter.h"
#include "ui/resource.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlmisc.h>

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

        NOTIFY_CODE_HANDLER(TTN_GETDISPINFOW, OnGetDispInfo)

        NOTIFY_HANDLER(kComputerListCtrl, NM_RCLICK, OnComputerListRightClick)
        NOTIFY_HANDLER(kGroupTreeCtrl, NM_RCLICK, OnGroupTreeRightClick)

        COMMAND_ID_HANDLER(ID_OPEN, OnOpenButton)
        COMMAND_ID_HANDLER(ID_SAVE, OnSaveButton)
        COMMAND_ID_HANDLER(ID_ABOUT, OnAboutButton)
        COMMAND_ID_HANDLER(ID_EXIT, OnExitButton)
        COMMAND_ID_HANDLER(ID_ADD_COMPUTER, OnAddComputerButton)
        COMMAND_ID_HANDLER(ID_ADD_GROUP, OnAddGroupButton)
        COMMAND_ID_HANDLER(ID_EDIT_COMPUTER, OnEditComputerButton)
        COMMAND_ID_HANDLER(ID_EDIT_GROUP, OnEditGroupButton)
        COMMAND_ID_HANDLER(ID_DELETE_COMPUTER, OnDeleteComputerButton)
        COMMAND_ID_HANDLER(ID_DELETE_GROUP, OnDeleteGroupButton)

        COMMAND_ID_HANDLER(ID_DESKTOP_MANAGE_SESSION, OnSessionButton)
        COMMAND_ID_HANDLER(ID_DESKTOP_VIEW_SESSION, OnSessionButton)
        COMMAND_ID_HANDLER(ID_FILE_TRANSFER_SESSION, OnSessionButton)
        COMMAND_ID_HANDLER(ID_SYSTEM_INFO_SESSION, OnSessionButton)
        COMMAND_ID_HANDLER(ID_POWER_SESSION, OnSessionButton)
    END_MSG_MAP()

    LRESULT OnCreate(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnDestroy(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnSize(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnGetMinMaxInfo(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);

    LRESULT OnGetDispInfo(int control_id, LPNMHDR hdr, BOOL& handled);
    LRESULT OnComputerListRightClick(int control_id, LPNMHDR hdr, BOOL& handled);
    LRESULT OnGroupTreeRightClick(int control_id, LPNMHDR hdr, BOOL& handled);

    LRESULT OnOpenButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnSaveButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnAboutButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnExitButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnAddComputerButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnAddGroupButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnEditComputerButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnEditGroupButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnDeleteComputerButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnDeleteGroupButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnSessionButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);

    void InitToolBar(const CSize& small_icon_size);
    void InitComputerList();
    void InitGroupTree();

    CIcon small_icon_;
    CIcon big_icon_;

    VerticalSplitter splitter_;
    CTreeViewCtrl group_tree_ctrl_;
    CListViewCtrl computer_list_ctrl_;
    CStatusBarCtrl statusbar_;
    CToolBarCtrl toolbar_;
    CImageListManaged toolbar_imagelist_;
};

} // namespace aspia

#endif // _ASPIA_UI__ADDRESS_BOOK_WINDOW_H
