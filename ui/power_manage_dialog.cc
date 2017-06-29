//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/power_manage_dialog.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/power_manage_dialog.h"
#include "ui/resource.h"
#include "proto/power_session.pb.h"

namespace aspia {

INT_PTR UiPowerManageDialog::DoModal(HWND parent)
{
    return Run(UiModule::Current(), parent, IDD_POWER);
}

INT_PTR UiPowerManageDialog::OnMessage(UINT msg, WPARAM wparam, LPARAM lparam)
{
    UNREF(lparam);

    switch (msg)
    {
        case WM_INITDIALOG:
        {
            CenterWindow();
            CheckDlgButton(ID_POWER_SHUTDOWN, BST_CHECKED);
            SetFocus(GetDlgItem(ID_POWER_SHUTDOWN));
        }
        break;

        case WM_COMMAND:
        {
            switch (LOWORD(wparam))
            {
                case IDOK:
                {
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
                }
                break;

                case IDCANCEL:
                    EndDialog(proto::PowerEvent::UNKNOWN);
                    break;
            }
        }
        break;

        case WM_CLOSE:
            EndDialog(proto::PowerEvent::UNKNOWN);
            break;
    }

    return 0;
}

} // namespace aspia
