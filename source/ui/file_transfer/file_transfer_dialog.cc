//
// PROJECT:         Aspia
// FILE:            ui/file_transfer/file_transfer_dialog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/file_transfer/file_transfer_dialog.h"
#include "base/logging.h"
#include "client/file_transfer_downloader.h"
#include "client/file_transfer_uploader.h"
#include "ui/file_transfer/file_action_dialog.h"
#include "ui/file_transfer/file_status_code.h"

#include <atlctrls.h>

namespace aspia {

FileTransferDialog::FileTransferDialog(Mode mode,
                                       std::shared_ptr<FileRequestSenderProxy> local_sender,
                                       std::shared_ptr<FileRequestSenderProxy> remote_sender,
                                       const FilePath& source_path,
                                       const FilePath& target_path,
                                       const FileTaskQueueBuilder::FileList& file_list)
    : mode_(mode),
      local_sender_(local_sender),
      remote_sender_(remote_sender),
      source_path_(source_path),
      target_path_(target_path),
      file_list_(file_list)
{
    runner_ = MessageLoopProxy::Current();
    DCHECK(runner_);
    DCHECK(runner_->BelongsToCurrentThread());
}

LRESULT FileTransferDialog::OnInitDialog(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    CenterWindow();

    current_item_edit_ = GetDlgItem(IDC_CURRENT_ITEM_EDIT);

    CString building_text;
    building_text.LoadStringW(IDS_FT_FILE_LIST_BUILDING);
    current_item_edit_.SetWindowTextW(building_text);

    total_progress_ = GetDlgItem(IDC_TOTAL_PROGRESS);
    total_progress_.SetMarquee(TRUE, 30);

    current_progress_ = GetDlgItem(IDC_CURRENT_PROGRESS);
    current_progress_.SetMarquee(TRUE, 30);

    if (mode_ == Mode::UPLOAD)
    {
        file_transfer_.reset(new FileTransferUploader(local_sender_, remote_sender_, this));
    }
    else
    {
        DCHECK(mode_ == Mode::DOWNLOAD);
        file_transfer_.reset(new FileTransferDownloader(local_sender_, remote_sender_, this));
    }

    file_transfer_->Start(source_path_, target_path_, file_list_);

    GetDlgItem(IDCANCEL).SetFocus();
    return TRUE;
}

LRESULT FileTransferDialog::OnClose(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    file_transfer_.reset();
    EndDialog(0);
    return 0;
}

LRESULT FileTransferDialog::OnCancelButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    PostMessageW(WM_CLOSE);
    return 0;
}

void FileTransferDialog::OnTransferStarted(uint64_t size)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&FileTransferDialog::OnTransferStarted, this, size));
        return;
    }

    total_progress_.ModifyStyle(PBS_MARQUEE, 0);
    total_progress_.SetRange(0, 100);
    total_progress_.SetPos(0);

    current_progress_.ModifyStyle(PBS_MARQUEE, 0);
    current_progress_.SetRange(0, 100);
    current_progress_.SetPos(0);

    // Save the total size of all the transferred objects.
    total_size_ = size;
    total_transferred_size_ = 0;
}

void FileTransferDialog::OnTransferComplete()
{
    PostMessageW(WM_CLOSE);
}

void FileTransferDialog::OnObjectTransferStarted(const FilePath& object_path, uint64_t object_size)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(
            &FileTransferDialog::OnObjectTransferStarted, this, object_path, object_size));
        return;
    }

    current_item_edit_.SetWindowTextW(object_path.c_str());
    object_size_ = object_size;
    object_transferred_size_ = 0;
}

void FileTransferDialog::OnObjectTransfer(uint64_t left_size)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&FileTransferDialog::OnObjectTransfer, this, left_size));
        return;
    }

    if (total_size_ != 0)
    {
        uint64_t transferred = 0;

        if (object_size_ != 0)
            transferred = object_size_ - left_size;

        total_transferred_size_ += transferred - object_transferred_size_;
        object_transferred_size_ = transferred;

        int percentage = static_cast<int>((total_transferred_size_ * 100ULL) / total_size_);
        total_progress_.SetPos(percentage);
    }

    if (object_size_ != 0)
    {
        int percentage = static_cast<int>((object_transferred_size_ * 100ULL) / object_size_);
        current_progress_.SetPos(percentage);
    }
}

void FileTransferDialog::OnFileOperationFailure(const FilePath& file_path,
                                                proto::file_transfer::Status status,
                                                FileTransfer::ActionCallback callback)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(
            &FileTransferDialog::OnFileOperationFailure, this, file_path, status, callback));
        return;
    }

    if (!file_transfer_)
        return;

    FileActionDialog dialog(file_path, status);
    dialog.DoModal(*this);
    callback(dialog.GetAction());
}

} // namespace aspia
