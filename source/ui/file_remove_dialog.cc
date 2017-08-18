//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_remove_dialog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/file_remove_dialog.h"
#include "base/logging.h"

namespace aspia {

FileRemoveDialog::FileRemoveDialog(std::shared_ptr<FileRequestSenderProxy> sender)
    : sender_(sender)
{
    runner_ = MessageLoopProxy::Current();
    DCHECK(runner_);
    DCHECK(runner_->BelongsToCurrentThread());
}

LRESULT FileRemoveDialog::OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    CenterWindow();

    current_item_edit_ = GetDlgItem(IDC_CURRENT_ITEM_EDIT);

    total_progress_ = GetDlgItem(IDC_TOTAL_PROGRESS);
    total_progress_.SetRange(0, 100);
    total_progress_.SetPos(0);

    current_progress_ = GetDlgItem(IDC_CURRENT_PROGRESS);
    current_progress_.SetRange(0, 100);
    current_progress_.SetPos(0);

    GetDlgItem(IDCANCEL).SetFocus();
    return FALSE;
}

LRESULT FileRemoveDialog::OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    EndDialog(0);
    return 0;
}

LRESULT FileRemoveDialog::OnCancelButton(WORD notify_code, WORD control_id, HWND control,
                                         BOOL& handled)
{
    PostMessageW(WM_CLOSE);
    return 0;
}

} // namespace aspia
