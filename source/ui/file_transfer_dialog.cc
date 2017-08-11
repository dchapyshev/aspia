//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_transfer_dialog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/file_transfer_dialog.h"
#include "base/logging.h"
#include "client/file_transfer_downloader.h"
#include "client/file_transfer_uploader.h"
#include "ui/file_action_dialog.h"
#include "ui/status_code.h"

#include <atlctrls.h>

namespace aspia {

UiFileTransferDialog::UiFileTransferDialog(Mode mode,
                                           std::shared_ptr<FileRequestSenderProxy> sender,
                                           const FilePath& source_path,
                                           const FilePath& target_path,
                                           const FileTransfer::FileList& file_list)
    : mode_(mode),
      sender_(std::move(sender)),
      source_path_(source_path),
      target_path_(target_path),
      file_list_(file_list)
{
    runner_ = MessageLoopProxy::Current();
    DCHECK(runner_);
    DCHECK(runner_->BelongsToCurrentThread());
}

LRESULT UiFileTransferDialog::OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    CenterWindow();

    current_item_edit_ = GetDlgItem(IDC_CURRENT_ITEM_EDIT);

    total_progress_ = GetDlgItem(IDC_TOTAL_PROGRESS);
    total_progress_.SetRange(0, 100);
    total_progress_.SetPos(0);

    current_progress_ = GetDlgItem(IDC_CURRENT_PROGRESS);
    current_progress_.SetRange(0, 100);
    current_progress_.SetPos(0);

    if (mode_ == Mode::UPLOAD)
    {
        file_transfer_.reset(new FileTransferUploader(sender_, this));
    }
    else
    {
        DCHECK(mode_ == Mode::DOWNLOAD);
        file_transfer_.reset(new FileTransferDownloader(sender_, this));
    }

    file_transfer_->Start(source_path_, target_path_, file_list_);

    GetDlgItem(IDCANCEL).SetFocus();
    return TRUE;
}

LRESULT UiFileTransferDialog::OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    file_transfer_.reset();
    EndDialog(0);
    return 0;
}

LRESULT UiFileTransferDialog::OnCancelButton(WORD code, WORD ctrl_id, HWND ctrl, BOOL& handled)
{
    PostMessageW(WM_CLOSE);
    return 0;
}

void UiFileTransferDialog::OnTransferStarted(uint64_t size)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&UiFileTransferDialog::OnTransferStarted, this, size));
        return;
    }

    if (!file_transfer_)
        return;

    // Save the total size of all the transferred objects.
    total_size_ = size;
    transferred_total_ = 0;
    transferred_per_object_ = 0;
}

void UiFileTransferDialog::OnTransferComplete()
{
    PostMessageW(WM_CLOSE);
}

void UiFileTransferDialog::OnObjectTransfer(const FilePath& source_path,
                                            uint64_t total_object_size,
                                            uint64_t left_object_size)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&UiFileTransferDialog::OnObjectTransfer, this, source_path,
                                    total_object_size, left_object_size));
        return;
    }

    if (!file_transfer_)
        return;

    current_item_edit_.SetWindowTextW(source_path.c_str());

    uint64_t transferred = total_object_size - left_object_size;

    if (total_size_ != 0)
    {
        transferred_total_ += transferred - transferred_per_object_;
        transferred_per_object_ = transferred;

        int percentage = static_cast<int>((transferred_total_ * 100ULL) / total_size_);
        total_progress_.SetPos(percentage);
    }

    if (total_object_size != 0)
    {
        int percentage = static_cast<int>((transferred * 100ULL) / total_object_size);
        current_progress_.SetPos(percentage);
    }

    if (left_object_size == 0)
        transferred_per_object_ = 0;
}

void UiFileTransferDialog::OnFileOperationFailure(const FilePath& file_path,
                                                  proto::RequestStatus status,
                                                  FileTransfer::ActionCallback callback)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(
            &UiFileTransferDialog::OnFileOperationFailure, this, file_path, status, callback));
        return;
    }

    if (!file_transfer_)
        return;

    UiFileActionDialog dialog(file_path, status);
    dialog.DoModal(*this);
    callback(dialog.GetAction());
}

} // namespace aspia
