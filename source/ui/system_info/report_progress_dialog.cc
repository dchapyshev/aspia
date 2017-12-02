//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/report_progress_dialog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/system_info/report_progress_dialog.h"

namespace aspia {

LRESULT ReportProgressDialog::OnInitDialog(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    CenterWindow();
    return FALSE;
}

LRESULT ReportProgressDialog::OnCancelButton(
    WORD /* notify_code */, WORD /* ctrl_id */, HWND /* ctrl */, BOOL& /* handled */)
{
    EndDialog(0);
    return 0;
}

} // namespace aspia

