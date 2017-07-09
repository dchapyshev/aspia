//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_transfer_dialog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/file_transfer_dialog.h"

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
    // TODO
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

void UiFileTransferDialog::OnDriveListRequestReply(std::unique_ptr<proto::DriveList> drive_list)
{
    // TODO
}

void UiFileTransferDialog::OnDriveListRequestFailure(proto::RequestStatus status)
{
    // TODO
}

void UiFileTransferDialog::OnFileListRequestReply(const FilePath& path,
                                                  std::unique_ptr<proto::FileList> file_list)
{
    // TODO
}

void UiFileTransferDialog::OnFileListRequestFailure(const FilePath& path,
                                                    proto::RequestStatus status)
{
    // TODO
}

void UiFileTransferDialog::OnDirectorySizeRequestReply(const FilePath& path, uint64_t size)
{
    // TODO
}

void UiFileTransferDialog::OnDirectorySizeRequestFailure(const FilePath& path,
                                                         proto::RequestStatus status)
{
    // TODO
}

void UiFileTransferDialog::OnCreateDirectoryRequestReply(const FilePath& path,
                                                         proto::RequestStatus status)
{
    // TODO
}

void UiFileTransferDialog::OnRemoveRequestReply(const FilePath& path,
                                                proto::RequestStatus status)
{
    // TODO
}

void UiFileTransferDialog::OnRenameRequestReply(const FilePath& old_name,
                                                const FilePath& new_name,
                                                proto::RequestStatus status)
{
    // TODO
}

} // namespace aspia
