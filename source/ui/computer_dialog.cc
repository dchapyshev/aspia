//
// PROJECT:         Aspia
// FILE:            ui/computer_dialog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/computer_dialog.h"

namespace aspia {

LRESULT ComputerDialog::OnInitDialog(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    DlgResize_Init();
    return FALSE;
}

LRESULT ComputerDialog::OnClose(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    EndDialog(0);
    return 0;
}

LRESULT ComputerDialog::OnOkButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    return 0;
}

LRESULT ComputerDialog::OnCancelButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    return 0;
}

} // namespace aspia
