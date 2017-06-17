//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_replace_dialog.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/file_replace_dialog.h"
#include "ui/resource.h"

namespace aspia {

INT_PTR FileReplaceDialog::DoModal(HWND parent)
{
    return Run(UiModule::Current(), parent, IDD_FILE_REPLACE);
}

INT_PTR FileReplaceDialog::OnMessage(UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
        case WM_INITDIALOG:
            break;

        case WM_COMMAND:
        {
            switch (LOWORD(wparam))
            {
                case IDCANCEL:
                    EndDialog();
                    break;
            }
        }
        break;

        case WM_CLOSE:
            EndDialog();
            break;
    }

    return 0;
}

} // namespace aspia
