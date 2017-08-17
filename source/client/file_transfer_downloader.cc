//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/file_transfer_downloader.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/file_transfer_downloader.h"
#include "base/logging.h"

namespace aspia {

FileTransferDownloader::FileTransferDownloader(std::shared_ptr<FileRequestSenderProxy> sender,
                                               FileTransfer::Delegate* delegate)
    : FileTransfer(std::move(sender), delegate)
{
    thread_.Start(MessageLoop::TYPE_DEFAULT, this);
}

FileTransferDownloader::~FileTransferDownloader()
{
    thread_.Stop();
}

void FileTransferDownloader::OnBeforeThreadRunning()
{
    runner_ = thread_.message_loop_proxy();
    DCHECK(runner_);
}

void FileTransferDownloader::OnAfterThreadRunning()
{
    delegate_->OnTransferComplete();
}

void FileTransferDownloader::Start(const FilePath& source_path,
                                   const FilePath& target_path,
                                   const FileList& file_list)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&FileTransferDownloader::Start, this,
                                    source_path, target_path, file_list));
        return;
    }

    // TODO
}

void FileTransferDownloader::OnUnableToCreateDirectoryAction(Action action)
{
    // TODO
}

void FileTransferDownloader::OnDriveListReply(std::unique_ptr<proto::DriveList> drive_list,
                                              proto::RequestStatus status)
{
    // TODO
}

void FileTransferDownloader::OnFileListReply(const FilePath& path,
                                             std::unique_ptr<proto::FileList> file_list,
                                             proto::RequestStatus status)
{
    // TODO
}

void FileTransferDownloader::OnDirectorySizeReply(const FilePath& path,
                                                  uint64_t size,
                                                  proto::RequestStatus status)
{
    // TODO
}

void FileTransferDownloader::OnCreateDirectoryReply(const FilePath& path,
                                                    proto::RequestStatus status)
{
    // TODO
}

void FileTransferDownloader::OnRemoveReply(const FilePath& path,
                                           proto::RequestStatus status)
{
    // TODO
}

void FileTransferDownloader::OnRenameReply(const FilePath& old_name,
                                           const FilePath& new_name,
                                           proto::RequestStatus status)
{
    // TODO
}

void FileTransferDownloader::OnFileUploadReply(const FilePath& file_path,
                                               proto::RequestStatus status)
{
    // TODO
}

void FileTransferDownloader::OnFileDownloadReply(const FilePath& file_path,
                                                 proto::RequestStatus status)
{
    // TODO
}

void FileTransferDownloader::OnFilePacketSended(uint32_t flags, proto::RequestStatus status)
{
    // TODO
}

void FileTransferDownloader::OnFilePacketReceived(std::unique_ptr<proto::FilePacket> file_packet,
                                                  proto::RequestStatus status)
{
    // TODO
}

} // namespace aspia
