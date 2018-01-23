//
// PROJECT:         Aspia
// FILE:            ui/computer_group_dialog.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__COMPUTER_GROUP_DIALOG_H
#define _ASPIA_UI__COMPUTER_GROUP_DIALOG_H

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlframe.h>

#include "ui/resource.h"

namespace aspia {

class ComputerGroupDialog :
    public CDialogImpl<ComputerGroupDialog>,
    public CDialogResize<ComputerGroupDialog>
{
public:
    enum { IDD = IDD_COMPUTER_GROUP };

    ComputerGroupDialog() = default;

private:
    BEGIN_MSG_MAP(ComputerGroupDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)

        COMMAND_ID_HANDLER(IDOK, OnOkButton)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancelButton)

        CHAIN_MSG_MAP(CDialogResize<ComputerGroupDialog>)
    END_MSG_MAP()

    BEGIN_DLGRESIZE_MAP(ComputerGroupDialog)
        DLGRESIZE_CONTROL(IDC_NAME_EDIT, DLSZ_SIZE_X)
        DLGRESIZE_CONTROL(IDC_COMMENT_EDIT, DLSZ_SIZE_X | DLSZ_SIZE_Y)
        DLGRESIZE_CONTROL(IDOK, DLSZ_MOVE_X | DLSZ_MOVE_Y)
        DLGRESIZE_CONTROL(IDCANCEL, DLSZ_MOVE_X | DLSZ_MOVE_Y)
    END_DLGRESIZE_MAP()

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnOkButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnCancelButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
};

} // namespace aspia

#endif // _ASPIA_UI__COMPUTER_GROUP_DIALOG_H
