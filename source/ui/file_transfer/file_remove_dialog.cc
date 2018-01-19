//
// PROJECT:         Aspia
// FILE:            ui/file_transfer/file_remove_dialog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/file_transfer/file_remove_dialog.h"
#include "ui/file_transfer/file_action_dialog.h"
#include "base/logging.h"

#include <atlmisc.h>

namespace aspia {

FileRemoveDialog::FileRemoveDialog(std::shared_ptr<FileRequestSenderProxy> sender,
                                   const std::experimental::filesystem::path& path,
                                   const FileTaskQueueBuilder::FileList& file_list)
    : sender_(sender),
      path_(path),
      file_list_(file_list)
{
    runner_ = MessageLoopProxy::Current();
    DCHECK(runner_);
    DCHECK(runner_->BelongsToCurrentThread());
}

LRESULT FileRemoveDialog::OnInitDialog(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    CenterWindow();

    current_item_edit_ = GetDlgItem(IDC_CURRENT_ITEM_EDIT);

    CString building_text;
    building_text.LoadStringW(IDS_FT_FILE_LIST_BUILDING);
    current_item_edit_.SetWindowTextW(building_text);

    total_progress_ = GetDlgItem(IDC_TOTAL_PROGRESS);
    total_progress_.SetMarquee(TRUE, 30);

    CProgressBarCtrl current_progress(GetDlgItem(IDC_CURRENT_PROGRESS));
    current_progress.SetMarquee(TRUE, 30);

    file_remover_.reset(new FileRemover(sender_, this));
    file_remover_->Start(path_, file_list_);

    GetDlgItem(IDCANCEL).SetFocus();
    return FALSE;
}

LRESULT FileRemoveDialog::OnClose(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    file_remover_.reset();
    EndDialog(0);
    return 0;
}

LRESULT FileRemoveDialog::OnCancelButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    PostMessageW(WM_CLOSE);
    return 0;
}

void FileRemoveDialog::OnRemovingStarted(int64_t object_count)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&FileRemoveDialog::OnRemovingStarted, this, object_count));
        return;
    }

    total_progress_.ModifyStyle(PBS_MARQUEE, 0);
    total_progress_.SetRange(0, 100);
    total_progress_.SetPos(0);

    total_object_count_ = object_count;
    object_count_ = 0;
}

void FileRemoveDialog::OnRemovingComplete()
{
    PostMessageW(WM_CLOSE);
}

void FileRemoveDialog::OnRemoveObject(const std::experimental::filesystem::path& object_path)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&FileRemoveDialog::OnRemoveObject, this, object_path));
        return;
    }

    current_item_edit_.SetWindowTextW(object_path.c_str());

    ++object_count_;

    if (total_object_count_ != 0)
    {
        int percentage = static_cast<int>((object_count_ * 100LL) / total_object_count_);
        total_progress_.SetPos(percentage);
    }
}

void FileRemoveDialog::OnRemoveObjectFailure(
    const std::experimental::filesystem::path& object_path,
    proto::file_transfer::Status status,
    FileRemover::ActionCallback callback)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&FileRemoveDialog::OnRemoveObjectFailure, this,
                                    object_path, status, callback));
        return;
    }

    FileActionDialog dialog(object_path, status);
    dialog.DoModal(*this);
    callback(dialog.GetAction());
}

} // namespace aspia
