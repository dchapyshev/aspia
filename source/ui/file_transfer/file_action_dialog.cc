//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_transfer/file_action_dialog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/file_transfer/file_action_dialog.h"
#include "ui/status_code.h"

namespace aspia {

FileActionDialog::FileActionDialog(const FilePath& path, proto::RequestStatus status)
    : path_(path),
      status_(status)
{
    // Nothing
}

LRESULT FileActionDialog::OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    CenterWindow();

    if (status_ != proto::REQUEST_STATUS_PATH_ALREADY_EXISTS)
    {
        GetDlgItem(IDC_REPLACE_BUTTON).EnableWindow(FALSE);
        GetDlgItem(IDC_REPLACE_ALL_BUTTON).EnableWindow(FALSE);
    }

    CString status = RequestStatusCodeToString(status_);

    CString text;
    text.Format(IDS_FT_OP_FAILURE, path_.c_str(), status.GetBuffer(0));

    GetDlgItem(IDC_CURRENT_ITEM_EDIT).SetWindowTextW(text);
    GetDlgItem(IDCANCEL).SetFocus();

    return FALSE;
}

LRESULT FileActionDialog::OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    EndDialog(0);
    return 0;
}

LRESULT FileActionDialog::OnReplaceButton(WORD notify_code, WORD control_id, HWND control,
                                          BOOL& handled)
{
    action_ = FileAction::REPLACE;
    EndDialog(0);
    return 0;
}

LRESULT FileActionDialog::OnReplaceAllButton(WORD notify_code, WORD control_id, HWND control,
                                             BOOL& handled)
{
    action_ = FileAction::REPLACE_ALL;
    EndDialog(0);
    return 0;
}

LRESULT FileActionDialog::OnSkipButton(WORD notify_code, WORD control_id, HWND control,
                                       BOOL& handled)
{
    action_ = FileAction::SKIP;
    EndDialog(0);
    return 0;
}

LRESULT FileActionDialog::OnSkipAllButton(WORD notify_code, WORD control_id, HWND control,
                                          BOOL& handled)
{
    action_ = FileAction::SKIP_ALL;
    EndDialog(0);
    return 0;
}

LRESULT FileActionDialog::OnCancelButton(WORD notify_code, WORD control_id, HWND control,
                                         BOOL& handled)
{
    EndDialog(0);
    return 0;
}

} // namespace aspia
