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

namespace aspia {

UiFileTransferDialog::UiFileTransferDialog(
    Mode mode,
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
    // Nothing
}

LRESULT UiFileTransferDialog::OnInitDialog(UINT message, WPARAM wparam,
                                           LPARAM lparam, BOOL& handled)
{
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

    return TRUE;
}

LRESULT UiFileTransferDialog::OnClose(UINT message, WPARAM wparam,
                                      LPARAM lparam, BOOL& handled)
{
    EndDialog(0);
    return 0;
}

LRESULT UiFileTransferDialog::OnCancelButton(WORD code, WORD ctrl_id,
                                             HWND ctrl, BOOL& handled)
{
    EndDialog(0);
    return 0;
}

void UiFileTransferDialog::OnObjectSizeNotify(uint64_t size)
{
    // TODO
}

void UiFileTransferDialog::OnTransferCompletionNotify()
{
    PostMessageW(WM_CLOSE);
}

void UiFileTransferDialog::OnObjectTransferNotify(
    const FilePath& source_path,
    const FilePath& target_path)
{
    // TODO
}

void UiFileTransferDialog::OnDirectoryOverwriteRequest(
    const FilePath& object_name,
    const FilePath& path)
{
    // TODO
}

void UiFileTransferDialog::OnFileOverwriteRequest(
    const FilePath& object_name,
    const FilePath& path)
{
    // TODO
}

} // namespace aspia
