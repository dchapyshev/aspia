//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/power_manage_dialog.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__POWER_MANAGE_DIALOG_H
#define _ASPIA_UI__POWER_MANAGE_DIALOG_H

#include "base/macros.h"
#include "ui/resource.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>

namespace aspia {

class UiPowerManageDialog : public CDialogImpl<UiPowerManageDialog>
{
public:
    enum { IDD = IDD_POWER };

    UiPowerManageDialog() = default;
    ~UiPowerManageDialog() = default;

private:
    BEGIN_MSG_MAP(UiPowerManageDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)

        COMMAND_ID_HANDLER(IDOK, OnOkButton)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancelButton)
    END_MSG_MAP()

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnOkButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnCancelButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);

    DISALLOW_COPY_AND_ASSIGN(UiPowerManageDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__POWER_MANAGE_DIALOG_H
