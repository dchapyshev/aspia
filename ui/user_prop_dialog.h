//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/user_prop_dialog.h
// LICENSE:         See top-level directory
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
#include <atlctrls.h>

namespace aspia {

class UiUserPropDialog : public CDialogImpl<UiUserPropDialog>
{
public:
    enum { IDD = IDD_USER_PROP };

    enum class Mode { Add, Edit };

    UiUserPropDialog(Mode mode,
                     proto::HostUser* user,
                     const HostUserList& user_list);

private:
    BEGIN_MSG_MAP(UiUserPropDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)

        COMMAND_ID_HANDLER(IDOK, OnOkButton)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancelButton)
    END_MSG_MAP()

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnOkButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnCancelButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    void OnPasswordEditDblClick();
    void ShowErrorMessage(UINT string_id);

    void InsertSessionType(CListViewCtrl& list,
                           proto::SessionType session_type,
                           UINT string_id);

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

    DISALLOW_COPY_AND_ASSIGN(UiUserPropDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__USER_PROP_DIALOG_H
