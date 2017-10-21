//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/power/power_manage_dialog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/power/power_manage_dialog.h"
#include "proto/power_session.pb.h"

#include <atlctrls.h>

namespace aspia {

LRESULT PowerManageDialog::OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    UNUSED_PARAMETER(message);
    UNUSED_PARAMETER(wparam);
    UNUSED_PARAMETER(lparam);
    UNUSED_PARAMETER(handled);

    CenterWindow();

    icon_ = AtlLoadIconImage(IDI_POWER_SURGE,
                             LR_CREATEDIBSECTION,
                             GetSystemMetrics(SM_CXICON),
                             GetSystemMetrics(SM_CYICON));

    CStatic(GetDlgItem(IDC_POWER_ICON)).SetIcon(icon_);

    CheckDlgButton(ID_POWER_SHUTDOWN, BST_CHECKED);
    GetDlgItem(ID_POWER_SHUTDOWN).SetFocus();
    return 0;
}

LRESULT PowerManageDialog::OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    UNUSED_PARAMETER(message);
    UNUSED_PARAMETER(wparam);
    UNUSED_PARAMETER(lparam);
    UNUSED_PARAMETER(handled);

    EndDialog(proto::PowerEvent::UNKNOWN);
    return 0;
}

LRESULT PowerManageDialog::OnOkButton(WORD notify_code, WORD control_id, HWND control,
                                      BOOL& handled)
{
    UNUSED_PARAMETER(notify_code);
    UNUSED_PARAMETER(control_id);
    UNUSED_PARAMETER(control);
    UNUSED_PARAMETER(handled);

    proto::PowerEvent::Action action = proto::PowerEvent::UNKNOWN;

    if (IsDlgButtonChecked(ID_POWER_SHUTDOWN) == BST_CHECKED)
        action = proto::PowerEvent::SHUTDOWN;
    else if (IsDlgButtonChecked(ID_POWER_REBOOT) == BST_CHECKED)
        action = proto::PowerEvent::REBOOT;
    else if (IsDlgButtonChecked(ID_POWER_HIBERNATE) == BST_CHECKED)
        action = proto::PowerEvent::HIBERNATE;
    else if (IsDlgButtonChecked(ID_POWER_SUSPEND) == BST_CHECKED)
        action = proto::PowerEvent::SUSPEND;

    EndDialog(action);
    return 0;
}

LRESULT PowerManageDialog::OnCancelButton(WORD notify_code, WORD control_id, HWND control,
                                          BOOL& handled)
{
    UNUSED_PARAMETER(notify_code);
    UNUSED_PARAMETER(control_id);
    UNUSED_PARAMETER(control);
    UNUSED_PARAMETER(handled);

    EndDialog(proto::PowerEvent::UNKNOWN);
    return 0;
}

} // namespace aspia
