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

INT_PTR PowerManageDialog::DoModal(HWND parent)
{
    return Run(Module::Current(), parent, IDD_POWER);
}

INT_PTR PowerManageDialog::OnMessage(UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
        case WM_INITDIALOG:
        {
            CenterWindow();
            CheckDlgButton(hwnd(), ID_POWER_SHUTDOWN, BST_CHECKED);
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

                    if (IsDlgButtonChecked(hwnd(), ID_POWER_SHUTDOWN) == BST_CHECKED)
                        action = proto::PowerEvent::SHUTDOWN;
                    else if (IsDlgButtonChecked(hwnd(), ID_POWER_REBOOT) == BST_CHECKED)
                        action = proto::PowerEvent::REBOOT;
                    else if (IsDlgButtonChecked(hwnd(), ID_POWER_HIBERNATE) == BST_CHECKED)
                        action = proto::PowerEvent::HIBERNATE;
                    else if (IsDlgButtonChecked(hwnd(), ID_POWER_SUSPEND) == BST_CHECKED)
                        action = proto::PowerEvent::SUSPEND;
                    else if (IsDlgButtonChecked(hwnd(), ID_POWER_LOGOFF) == BST_CHECKED)
                        action = proto::PowerEvent::LOGOFF;
                    else if (IsDlgButtonChecked(hwnd(), ID_POWER_LOCK) == BST_CHECKED)
                        action = proto::PowerEvent::LOCK;

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
