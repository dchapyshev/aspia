//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/user_prop_dialog.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__USER_PROP_DIALOG_H
#define _ASPIA_UI__USER_PROP_DIALOG_H

#include "host/host_user_list.h"
#include "proto/auth_session.pb.h"
#include "ui/resource.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlframe.h>
#include <atlctrls.h>

namespace aspia {

class UserPropDialog :
    public CDialogImpl<UserPropDialog>,
    public CDialogResize<UserPropDialog>
{
public:
    enum { IDD = IDD_USER_PROP };

    enum class Mode { ADD, EDIT };

    UserPropDialog(Mode mode, proto::HostUser* user, const HostUserList& user_list);

private:
    BEGIN_MSG_MAP(UserPropDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)
        MESSAGE_HANDLER(WM_SIZE, OnSize)

        COMMAND_ID_HANDLER(IDOK, OnOkButton)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancelButton)

        CHAIN_MSG_MAP(CDialogResize<UserPropDialog>)
    END_MSG_MAP()

    BEGIN_DLGRESIZE_MAP(UiUserPropDialog)
        DLGRESIZE_CONTROL(IDC_USERNAME_EDIT, DLSZ_SIZE_X)
        DLGRESIZE_CONTROL(IDC_PASSWORD_EDIT, DLSZ_SIZE_X)
        DLGRESIZE_CONTROL(IDC_PASSWORD_RETRY_EDIT, DLSZ_SIZE_X)
        DLGRESIZE_CONTROL(IDC_SESSION_TYPES_LIST, DLSZ_SIZE_X | DLSZ_SIZE_Y)
        DLGRESIZE_CONTROL(IDOK, DLSZ_MOVE_X | DLSZ_MOVE_Y)
        DLGRESIZE_CONTROL(IDCANCEL, DLSZ_MOVE_X | DLSZ_MOVE_Y)
    END_DLGRESIZE_MAP()

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnSize(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnOkButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnCancelButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    void OnPasswordEditDblClick();
    void ShowErrorMessage(UINT string_id);
    void InsertSessionType(CListViewCtrl& list, proto::SessionType session_type, UINT string_id);

    static LRESULT CALLBACK PasswordEditWindowProc(HWND hwnd,
                                                   UINT msg,
                                                   WPARAM wparam,
                                                   LPARAM lparam,
                                                   UINT_PTR subclass_id,
                                                   DWORD_PTR ref_data);
    const Mode mode_;
    proto::HostUser* user_;
    const HostUserList& user_list_;
    bool password_changed_ = true;

    DISALLOW_COPY_AND_ASSIGN(UserPropDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__USER_PROP_DIALOG_H
