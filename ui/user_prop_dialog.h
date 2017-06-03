//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/user_prop_dialog.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__USER_PROP_DIALOG_H
#define _ASPIA_UI__USER_PROP_DIALOG_H

#include "ui/base/modal_dialog.h"
#include "ui/base/listview.h"
#include "proto/auth_session.pb.h"
#include "proto/host_user.pb.h"

namespace aspia {

class UserPropDialog : public ModalDialog
{
public:
    enum class Mode { Add, Edit };

    UserPropDialog(Mode mode,
                   proto::HostUser* user,
                   const proto::HostUserList& user_list);

    INT_PTR DoModal(HWND parent) override;

private:
    INT_PTR OnMessage(UINT msg, WPARAM wparam, LPARAM lparam) override;

    void OnInitDialog();
    void OnOkButton();
    void OnPasswordEditDblClick();

    void InsertSessionType(ListView& list,
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
    const proto::HostUserList& user_list_;
    bool password_changed_ = true;

    DISALLOW_COPY_AND_ASSIGN(UserPropDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__USER_PROP_DIALOG_H
