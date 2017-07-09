//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_transfer_dialog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/file_transfer_dialog.h"
#include "client/file_transfer_downloader.h"
#include "client/file_transfer_uploader.h"

namespace aspia {

UiFileTransferDialog::UiFileTransferDialog(std::shared_ptr<FileRequestSenderProxy> sender,
                                           const FilePath& path,
                                           const std::vector<proto::FileList::Item>& file_list) :
    sender_(std::move(sender)),
    path_(path),
    file_list_(file_list)
{
    // Nothing
}

LRESULT UiFileTransferDialog::OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    file_transfer_.reset(new FileTransferUploader(sender_, this));

    return TRUE;
}

LRESULT UiFileTransferDialog::OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    EndDialog(0);
    return 0;
}

LRESULT UiFileTransferDialog::OnCancelButton(WORD code, WORD ctrl_id, HWND ctrl, BOOL& handled)
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

void UiFileTransferDialog::OnObjectTransferNotify(const FilePath& source_path,
                                                  const FilePath& target_path)
{
    // TODO
}

void UiFileTransferDialog::OnDirectoryOverwriteRequest(const FilePath& object_name,
                                                       const FilePath& path)
{
    // TODO
}

void UiFileTransferDialog::OnFileOverwriteRequest(const FilePath& object_name,
                                                  const FilePath& path)
{
    // TODO
}

} // namespace aspia
