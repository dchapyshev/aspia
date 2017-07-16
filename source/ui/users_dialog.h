//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/users_dialog.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__USERS_DIALOG_H
#define _ASPIA_UI__USERS_DIALOG_H

#include "host/host_user_list.h"
#include "ui/resource.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlframe.h>

namespace aspia {

class UiUsersDialog :
    public CDialogImpl<UiUsersDialog>,
    public CDialogResize<UiUsersDialog>
{
public:
    enum { IDD = IDD_USERS };

    UiUsersDialog() = default;

private:
    BEGIN_MSG_MAP(UiUsersDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)
        MESSAGE_HANDLER(WM_SIZE, OnSize)

        COMMAND_ID_HANDLER(ID_ADD, OnAddButton)
        COMMAND_ID_HANDLER(ID_EDIT, OnEditButton)
        COMMAND_ID_HANDLER(ID_DELETE, OnDeleteButton)
        COMMAND_ID_HANDLER(IDOK, OnOkButton)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancelButton)

        NOTIFY_HANDLER(IDC_USER_LIST, NM_DBLCLK, OnUserListDoubleClick)
        NOTIFY_HANDLER(IDC_USER_LIST, NM_RCLICK, OnUserListRightClick)
        NOTIFY_HANDLER(IDC_USER_LIST, NM_CLICK, OnUserListClick)
        NOTIFY_HANDLER(IDC_USER_LIST, LVN_KEYDOWN, OnUserListKeyDown)
        NOTIFY_HANDLER(IDC_USER_LIST, LVN_ITEMCHANGED, OnUserListItemChanged)

        CHAIN_MSG_MAP(CDialogResize<UiUsersDialog>)
    END_MSG_MAP()

    BEGIN_DLGRESIZE_MAP(UiUsersDialog)
        DLGRESIZE_CONTROL(IDC_USER_LIST, DLSZ_SIZE_X | DLSZ_SIZE_Y)
        DLGRESIZE_CONTROL(IDOK, DLSZ_MOVE_X | DLSZ_MOVE_Y)
        DLGRESIZE_CONTROL(IDCANCEL, DLSZ_MOVE_X | DLSZ_MOVE_Y)
    END_DLGRESIZE_MAP()

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnSize(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);

    LRESULT OnAddButton(WORD code, WORD ctrl_id, HWND ctrl, BOOL& handled);
    LRESULT OnEditButton(WORD code, WORD ctrl_id, HWND ctrl, BOOL& handled);
    LRESULT OnDeleteButton(WORD code, WORD ctrl_id, HWND ctrl, BOOL& handled);
    LRESULT OnOkButton(WORD code, WORD ctrl_id, HWND ctrl, BOOL& handled);
    LRESULT OnCancelButton(WORD code, WORD ctrl_id, HWND ctrl, BOOL& handled);

    LRESULT OnUserListDoubleClick(int ctrl_id, LPNMHDR hdr, BOOL& handled);
    LRESULT OnUserListRightClick(int ctrl_id, LPNMHDR hdr, BOOL& handled);
    LRESULT OnUserListClick(int ctrl_id, LPNMHDR hdr, BOOL& handled);
    LRESULT OnUserListKeyDown(int ctrl_id, LPNMHDR hdr, BOOL& handled);
    LRESULT OnUserListItemChanged(int ctrl_id, LPNMHDR hdr, BOOL& handled);

    void UpdateUserList();
    int GetSelectedUserIndex();
    void ShowUserPopupMenu();
    void UpdateButtonsState();
    void SetUserListModified();
    void DeleteSelectedUser();
    void EditSelectedUser();

    HostUserList user_list_;
    CImageListManaged imagelist_;
    CIcon add_icon_;
    CIcon edit_icon_;
    CIcon delete_icon_;

    DISALLOW_COPY_AND_ASSIGN(UiUsersDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__USERS_DIALOG_H
