//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/file_request_sender_local.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/file_request_sender_local.h"
#include "base/logging.h"
#include "protocol/filesystem.h"

namespace aspia {

FileRequestSenderLocal::FileRequestSenderLocal()
{
    worker_thread_.Start(MessageLoop::Type::TYPE_DEFAULT, this);
}

FileRequestSenderLocal::~FileRequestSenderLocal()
{
    worker_thread_.Stop();
}

void FileRequestSenderLocal::OnBeforeThreadRunning()
{
    worker_ = worker_thread_.message_loop_proxy();
}

void FileRequestSenderLocal::OnAfterThreadRunning()
{
    // Nothing
}

void FileRequestSenderLocal::DriveListRequest(std::shared_ptr<FileReplyReceiverProxy> receiver)
{
    DCHECK(worker_->BelongsToCurrentThread());

    std::unique_ptr<proto::DriveList> drive_list = std::make_unique<proto::DriveList>();
    proto::RequestStatus status = ExecuteDriveListRequest(drive_list.get());

    receiver->OnDriveListReply(std::move(drive_list), status);
}

void FileRequestSenderLocal::SendDriveListRequest(std::shared_ptr<FileReplyReceiverProxy> receiver)
{
    worker_->PostTask(std::bind(&FileRequestSenderLocal::DriveListRequest, this, receiver));
}

void FileRequestSenderLocal::FileListRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                             const FilePath& path)
{
    DCHECK(worker_->BelongsToCurrentThread());

    std::unique_ptr<proto::FileList> file_list = std::make_unique<proto::FileList>();
    proto::RequestStatus status = ExecuteFileListRequest(path, file_list.get());

    receiver->OnFileListReply(path, std::move(file_list), status);
}

void FileRequestSenderLocal::SendFileListRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                                 const FilePath& path)
{
    worker_->PostTask(std::bind(&FileRequestSenderLocal::FileListRequest, this, receiver, path));
}

void FileRequestSenderLocal::CreateDirectoryRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const FilePath& path)
{
    DCHECK(worker_->BelongsToCurrentThread());
    receiver->OnCreateDirectoryReply(path, ExecuteCreateDirectoryRequest(path));
}

void FileRequestSenderLocal::SendCreateDirectoryRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const FilePath& path)
{
    worker_->PostTask(std::bind(&FileRequestSenderLocal::CreateDirectoryRequest, this, receiver, path));
}

void FileRequestSenderLocal::DirectorySizeRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                                  const FilePath& path)
{
    DCHECK(worker_->BelongsToCurrentThread());
    // TODO
}

void FileRequestSenderLocal::SendDirectorySizeRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const FilePath& path)
{
    worker_->PostTask(std::bind(&FileRequestSenderLocal::DirectorySizeRequest, this, receiver, path));
}

void FileRequestSenderLocal::RemoveRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                           const FilePath& path)
{
    DCHECK(worker_->BelongsToCurrentThread());
    receiver->OnRemoveReply(path, ExecuteRemoveRequest(path));
}

void FileRequestSenderLocal::SendRemoveRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                               const FilePath& path)
{
    worker_->PostTask(std::bind(&FileRequestSenderLocal::RemoveRequest, this, receiver, path));
}

void FileRequestSenderLocal::RenameRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                           const FilePath& old_name,
                                           const FilePath& new_name)
{
    DCHECK(worker_->BelongsToCurrentThread());
    receiver->OnRenameReply(old_name, new_name, ExecuteRenameRequest(old_name, new_name));
}

void FileRequestSenderLocal::SendRenameRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                               const FilePath& old_name,
                                               const FilePath& new_name)
{
    worker_->PostTask(std::bind(&FileRequestSenderLocal::RenameRequest,
                                this,
                                receiver,
                                old_name,
                                new_name));
}

void FileRequestSenderLocal::SendFileUploadRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                                   const FilePath& file_path,
                                                   Overwrite overwrite)
{
    DLOG(FATAL) << "The request type is not allowed for local processing";
}

void FileRequestSenderLocal::SendFileDownloadRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const FilePath& file_path)
{
    DLOG(FATAL) << "The request type is not allowed for local processing";
}

void FileRequestSenderLocal::SendFilePacket(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                            std::unique_ptr<proto::FilePacket> file_packet)
{
    DLOG(FATAL) << "The request type is not allowed for local processing";
}

void FileRequestSenderLocal::SendFilePacketRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver)
{
    DLOG(FATAL) << "The request type is not allowed for local processing";
}

} // namespace aspia
