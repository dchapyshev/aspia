//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/users_dialog.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__USERS_DIALOG_H
#define _ASPIA_UI__USERS_DIALOG_H

#include "ui/base/modal_dialog.h"
#include "host/host_user_list.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlmisc.h>

namespace aspia {

class UiUsersDialog : public UiModalDialog
{
public:
    UiUsersDialog() = default;

    INT_PTR DoModal(HWND parent) override;

private:
    INT_PTR OnMessage(UINT msg, WPARAM wparam, LPARAM lparam) override;

    void OnInitDialog();
    void OnAddButton();
    void OnEditButton();
    void OnDeleteButton();
    void OnOkButton();

    void UpdateUserList();
    int GetSelectedUserIndex();
    void ShowUserPopupMenu();
    void OnUserListClicked();
    void SetUserListModified();

    HostUserList user_list_;
    CImageListManaged imagelist_;

    DISALLOW_COPY_AND_ASSIGN(UiUsersDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__USERS_DIALOG_H
