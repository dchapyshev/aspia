//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/users_dialog.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__USERS_DIALOG_H
#define _ASPIA_UI__USERS_DIALOG_H

#include "host/host_user_list.h"
#include "ui/base/imagelist.h"
#include "ui/resource.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlmisc.h>

namespace aspia {

class UiUsersDialog : public CDialogImpl<UiUsersDialog>
{
public:
    enum { IDD = IDD_USERS };

    UiUsersDialog() = default;

private:
    BEGIN_MSG_MAP(UiUsersDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)

        COMMAND_ID_HANDLER(ID_ADD, OnAddButton)
        COMMAND_ID_HANDLER(ID_EDIT, OnEditButton)
        COMMAND_ID_HANDLER(ID_DELETE, OnDeleteButton)
        COMMAND_ID_HANDLER(IDOK, OnOkButton)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancelButton)

        NOTIFY_HANDLER(IDC_USER_LIST, NM_DBLCLK, OnUserListDoubleClick)
        NOTIFY_HANDLER(IDC_USER_LIST, NM_RCLICK, OnUserListRightClick)
        NOTIFY_HANDLER(IDC_USER_LIST, NM_CLICK, OnUserListClick)
        NOTIFY_HANDLER(IDC_USER_LIST, LVN_KEYDOWN, OnUserListKeyDown)
    END_MSG_MAP()

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);

    LRESULT OnAddButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnEditButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnDeleteButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnOkButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnCancelButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);

    LRESULT OnUserListDoubleClick(int control_id, LPNMHDR hdr, BOOL& handled);
    LRESULT OnUserListRightClick(int control_id, LPNMHDR hdr, BOOL& handled);
    LRESULT OnUserListClick(int control_id, LPNMHDR hdr, BOOL& handled);
    LRESULT OnUserListKeyDown(int control_id, LPNMHDR hdr, BOOL& handled);

    void UpdateUserList();
    int GetSelectedUserIndex();
    void ShowUserPopupMenu();
    void OnUserSelect();
    void SetUserListModified();
    void DeleteSelectedUser();
    void EditSelectedUser();

    HostUserList user_list_;
    CImageListCustom imagelist_;

    DISALLOW_COPY_AND_ASSIGN(UiUsersDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__USERS_DIALOG_H
