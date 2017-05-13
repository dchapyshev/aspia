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
            CenterWindow(GetParent(hwnd()));
            break;

        case WM_COMMAND:
        {
            switch (LOWORD(wparam))
            {
                case IDCANCEL:
                    EndDialog(proto::PowerEvent::UNKNOWN);
                    break;

                case ID_POWER_SHUTDOWN:
                    EndDialog(proto::PowerEvent::SHUTDOWN);
                    break;

                case ID_POWER_REBOOT:
                    EndDialog(proto::PowerEvent::REBOOT);
                    break;

                case ID_POWER_HIBERNATE:
                    EndDialog(proto::PowerEvent::HIBERNATE);
                    break;

                case ID_POWER_SUSPEND:
                    EndDialog(proto::PowerEvent::SUSPEND);
                    break;

                case ID_POWER_LOGOFF:
                    EndDialog(proto::PowerEvent::LOGOFF);
                    break;

                case ID_POWER_LOCK:
                    EndDialog(proto::PowerEvent::LOCK);
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
