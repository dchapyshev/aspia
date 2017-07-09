//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/file_transfer_downloader.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/file_transfer_downloader.h"

namespace aspia {

FileTransferDownloader::FileTransferDownloader(std::shared_ptr<FileRequestSenderProxy> sender,
                                               Delegate* delegate) :
    FileTransfer(std::move(sender), delegate)
{
    // Nothing
}

void FileTransferDownloader::OnDriveListRequestReply(std::unique_ptr<proto::DriveList> drive_list)
{
    // TODO
}

void FileTransferDownloader::OnDriveListRequestFailure(proto::RequestStatus status)
{
    // TODO
}

void FileTransferDownloader::OnFileListRequestReply(const FilePath& path,
                                                    std::unique_ptr<proto::FileList> file_list)
{
    // TODO
}

void FileTransferDownloader::OnFileListRequestFailure(const FilePath& path,
                                                      proto::RequestStatus status)
{
    // TODO
}

void FileTransferDownloader::OnDirectorySizeRequestReply(const FilePath& path,
                                 uint64_t size)
{
    // TODO
}

void FileTransferDownloader::OnDirectorySizeRequestFailure(const FilePath& path,
                                                           proto::RequestStatus status)
{
    // TODO
}

void FileTransferDownloader::OnCreateDirectoryRequestReply(const FilePath& path,
                                                           proto::RequestStatus status)
{
    // TODO
}

void FileTransferDownloader::OnRemoveRequestReply(const FilePath& path,
                                                  proto::RequestStatus status)
{
    // TODO
}

void FileTransferDownloader::OnRenameRequestReply(const FilePath& old_name,
                                                  const FilePath& new_name,
                                                  proto::RequestStatus status)
{
    // TODO
}

} // namespace aspia
