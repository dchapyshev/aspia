//
// PROJECT:         Aspia Remote Desktop
// FILE:            gui/users_dialog.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_GUI__USERS_DIALOG_H
#define _ASPIA_GUI__USERS_DIALOG_H

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlctrlx.h>
#include <atlcrack.h>
#include <atlmisc.h>

#include <list>

#include "gui/resource.h"

#include "host/host_user.h"

namespace aspia {

class UsersDialog : public CDialogImpl<UsersDialog>
{
public:
    UsersDialog() = default;
    ~UsersDialog() = default;

    enum { IDD = IDD_USERS };

private:
    BEGIN_MSG_MAP(AuthDialog)
        MSG_WM_INITDIALOG(OnInitDialog)
        MSG_WM_CLOSE(OnClose)

        COMMAND_ID_HANDLER(IDÑ_ADD_USER, OnAddButton)
        COMMAND_ID_HANDLER(IDC_EDIT_USER, OnEditButton)
        COMMAND_ID_HANDLER(IDC_DELETE_USER, OnDeleteButton)

        COMMAND_ID_HANDLER(IDOK, OnOkButton)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancelButton)
    END_MSG_MAP()

    BOOL OnInitDialog(CWindow focus_window, LPARAM lParam);
    void OnClose();
    LRESULT OnAddButton(WORD notify_code, WORD id, HWND hWndCtl, BOOL& handled);
    LRESULT OnEditButton(WORD notify_code, WORD id, HWND hWndCtl, BOOL& handled);
    LRESULT OnDeleteButton(WORD notify_code, WORD id, HWND hWndCtl, BOOL& handled);
    LRESULT OnOkButton(WORD notify_code, WORD id, HWND hWndCtl, BOOL& handled);
    LRESULT OnCancelButton(WORD notify_code, WORD id, HWND hWndCtl, BOOL& handled);

private:
    std::list<HostUser> user_list_;
};

} // namespace aspia

#endif // _ASPIA_GUI__USERS_DIALOG_H
