//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/save_report_dialog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/system_info/save_report_dialog.h"
#include <atlctrls.h>

namespace aspia {

LRESULT SaveReportDialog::OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    DlgResize_Init();
    CenterWindow();

    CSize small_icon_size(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));

    small_icon_ = AtlLoadIconImage(IDI_MAIN,
                                   LR_CREATEDIBSECTION,
                                   small_icon_size.cx,
                                   small_icon_size.cy);
    SetIcon(small_icon_, FALSE);

    big_icon_ = AtlLoadIconImage(IDI_MAIN,
                                 LR_CREATEDIBSECTION,
                                 GetSystemMetrics(SM_CXICON),
                                 GetSystemMetrics(SM_CYICON));
    SetIcon(big_icon_, TRUE);

    select_all_icon_ = AtlLoadIconImage(IDI_SELECT_ALL,
                                        LR_CREATEDIBSECTION,
                                        small_icon_size.cx,
                                        small_icon_size.cy);
    CButton(GetDlgItem(IDC_SELECT_ALL)).SetIcon(select_all_icon_);

    unselect_all_icon_ = AtlLoadIconImage(IDI_UNSELECT_ALL,
                                          LR_CREATEDIBSECTION,
                                          small_icon_size.cx,
                                          small_icon_size.cy);
    CButton(GetDlgItem(IDC_UNSELECT_ALL)).SetIcon(unselect_all_icon_);

    return FALSE;
}

LRESULT SaveReportDialog::OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    EndDialog(0);
    return 0;
}

LRESULT SaveReportDialog::OnSaveButton(WORD notify_code, WORD ctrl_id, HWND ctrl, BOOL& handled)
{
    return 0;
}

LRESULT SaveReportDialog::OnCancelButton(WORD notify_code, WORD ctrl_id, HWND ctrl, BOOL& handled)
{
    EndDialog(0);
    return 0;
}

} // namespace aspia
