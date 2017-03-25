//
// PROJECT:         Aspia Remote Desktop
// FILE:            gui/add_user_dialog.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_GUI__ADD_USER_DIALOG_H
#define _ASPIA_GUI__ADD_USER_DIALOG_H

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlctrlx.h>
#include <atlcrack.h>
#include <atlmisc.h>

#include "gui/resource.h"

namespace aspia {

class AddUserDialog : public CDialogImpl<AddUserDialog>
{
public:
    AddUserDialog() = default;
    ~AddUserDialog() = default;

    enum { IDD = IDD_USER_ADD };

private:
    BEGIN_MSG_MAP(AuthDialog)
        MSG_WM_INITDIALOG(OnInitDialog)
        MSG_WM_CLOSE(OnClose)

        COMMAND_ID_HANDLER(IDOK, OnOkButton)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancelButton)
    END_MSG_MAP()

    BOOL OnInitDialog(CWindow focus_window, LPARAM lParam);
    void OnClose();
    LRESULT OnOkButton(WORD notify_code, WORD id, HWND hWndCtl, BOOL& handled);
    LRESULT OnCancelButton(WORD notify_code, WORD id, HWND hWndCtl, BOOL& handled);
};

} // namespace aspia

#endif // _ASPIA_GUI__ADD_USER_DIALOG_H
