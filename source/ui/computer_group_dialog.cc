//
// PROJECT:         Aspia
// FILE:            ui/computer_group_dialog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/computer_group_dialog.h"

namespace aspia {

LRESULT ComputerGroupDialog::OnInitDialog(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    DlgResize_Init();
    return FALSE;
}

LRESULT ComputerGroupDialog::OnClose(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    EndDialog(0);
    return 0;
}

LRESULT ComputerGroupDialog::OnOkButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    return 0;
}

LRESULT ComputerGroupDialog::OnCancelButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    return 0;
}

} // namespace aspia
