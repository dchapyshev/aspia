//
// PROJECT:         Aspia
// FILE:            ui/computer_dialog.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__COMPUTER_DIALOG_H
#define _ASPIA_UI__COMPUTER_DIALOG_H

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlframe.h>

#include "ui/resource.h"

namespace aspia {

class ComputerDialog :
    public CDialogImpl<ComputerDialog>,
    public CDialogResize<ComputerDialog>
{
public:
    enum { IDD = IDD_COMPUTER };

    ComputerDialog() = default;

private:
    BEGIN_MSG_MAP(ComputerDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)

        COMMAND_ID_HANDLER(IDOK, OnOkButton)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancelButton)

        CHAIN_MSG_MAP(CDialogResize<ComputerDialog>)
    END_MSG_MAP()

    BEGIN_DLGRESIZE_MAP(ComputerDialog)
        DLGRESIZE_CONTROL(IDC_NAME_EDIT, DLSZ_SIZE_X)
        DLGRESIZE_CONTROL(IDC_SERVER_ADDRESS_EDIT, DLSZ_SIZE_X)
        DLGRESIZE_CONTROL(IDC_SERVER_PORT_EDIT, DLSZ_SIZE_X)
        DLGRESIZE_CONTROL(IDC_SESSION_TYPE_COMBO, DLSZ_SIZE_X)
        DLGRESIZE_CONTROL(IDC_SETTINGS_BUTTON, DLSZ_MOVE_X)
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

#endif // _ASPIA_UI__COMPUTER_DIALOG_H
