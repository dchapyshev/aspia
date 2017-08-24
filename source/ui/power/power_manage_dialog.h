//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/power/power_manage_dialog.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__POWER__POWER_MANAGE_DIALOG_H
#define _ASPIA_UI__POWER__POWER_MANAGE_DIALOG_H

#include "base/macros.h"
#include "ui/resource.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>

namespace aspia {

class PowerManageDialog : public CDialogImpl<PowerManageDialog>
{
public:
    enum { IDD = IDD_POWER };

    PowerManageDialog() = default;
    ~PowerManageDialog() = default;

private:
    BEGIN_MSG_MAP(PowerManageDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)

        COMMAND_ID_HANDLER(IDOK, OnOkButton)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancelButton)
    END_MSG_MAP()

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnOkButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnCancelButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);

    CIcon icon_;

    DISALLOW_COPY_AND_ASSIGN(PowerManageDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__POWER__POWER_MANAGE_DIALOG_H
