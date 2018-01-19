//
// PROJECT:         Aspia
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
    DCHECK(worker_);
}

void FileRequestSenderLocal::OnAfterThreadRunning()
{
    // Nothing
}

void FileRequestSenderLocal::DriveListRequest(std::shared_ptr<FileReplyReceiverProxy> receiver)
{
    DCHECK(worker_->BelongsToCurrentThread());

    std::unique_ptr<proto::file_transfer::DriveList> drive_list =
        std::make_unique<proto::file_transfer::DriveList>();

    proto::file_transfer::Status status = ExecuteDriveListRequest(drive_list.get());

    receiver->OnDriveListReply(std::move(drive_list), status);
}

void FileRequestSenderLocal::SendDriveListRequest(std::shared_ptr<FileReplyReceiverProxy> receiver)
{
    worker_->PostTask(std::bind(&FileRequestSenderLocal::DriveListRequest, this, receiver));
}

void FileRequestSenderLocal::FileListRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const std::experimental::filesystem::path& path)
{
    DCHECK(worker_->BelongsToCurrentThread());

    std::unique_ptr<proto::file_transfer::FileList> file_list =
        std::make_unique<proto::file_transfer::FileList>();

    proto::file_transfer::Status status = ExecuteFileListRequest(path, file_list.get());

    receiver->OnFileListReply(path, std::move(file_list), status);
}

void FileRequestSenderLocal::SendFileListRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const std::experimental::filesystem::path& path)
{
    worker_->PostTask(std::bind(&FileRequestSenderLocal::FileListRequest, this, receiver, path));
}

void FileRequestSenderLocal::CreateDirectoryRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const std::experimental::filesystem::path& path)
{
    DCHECK(worker_->BelongsToCurrentThread());
    receiver->OnCreateDirectoryReply(path, ExecuteCreateDirectoryRequest(path));
}

void FileRequestSenderLocal::SendCreateDirectoryRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const std::experimental::filesystem::path& path)
{
    worker_->PostTask(std::bind(&FileRequestSenderLocal::CreateDirectoryRequest, this, receiver, path));
}

void FileRequestSenderLocal::DirectorySizeRequest(
    std::shared_ptr<FileReplyReceiverProxy> /* receiver */,
    const std::experimental::filesystem::path& /* path */)
{
    DCHECK(worker_->BelongsToCurrentThread());
    // TODO
}

void FileRequestSenderLocal::SendDirectorySizeRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const std::experimental::filesystem::path& path)
{
    worker_->PostTask(std::bind(&FileRequestSenderLocal::DirectorySizeRequest, this, receiver, path));
}

void FileRequestSenderLocal::RemoveRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                           const std::experimental::filesystem::path& path)
{
    DCHECK(worker_->BelongsToCurrentThread());
    receiver->OnRemoveReply(path, ExecuteRemoveRequest(path));
}

void FileRequestSenderLocal::SendRemoveRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const std::experimental::filesystem::path& path)
{
    worker_->PostTask(std::bind(&FileRequestSenderLocal::RemoveRequest, this, receiver, path));
}

void FileRequestSenderLocal::RenameRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const std::experimental::filesystem::path& old_name,
    const std::experimental::filesystem::path& new_name)
{
    DCHECK(worker_->BelongsToCurrentThread());
    receiver->OnRenameReply(old_name, new_name, ExecuteRenameRequest(old_name, new_name));
}

void FileRequestSenderLocal::SendRenameRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const std::experimental::filesystem::path& old_name,
    const std::experimental::filesystem::path& new_name)
{
    worker_->PostTask(std::bind(&FileRequestSenderLocal::RenameRequest,
                                this,
                                receiver,
                                old_name,
                                new_name));
}

void FileRequestSenderLocal::SendFileUploadRequest(
    std::shared_ptr<FileReplyReceiverProxy> /* receiver */,
    const std::experimental::filesystem::path& /* file_path */,
    Overwrite /* overwrite */)
{
    DLOG(FATAL) << "The request type is not allowed for local processing";
}

void FileRequestSenderLocal::SendFileDownloadRequest(
    std::shared_ptr<FileReplyReceiverProxy> /* receiver */,
    const std::experimental::filesystem::path& /* file_path */)
{
    DLOG(FATAL) << "The request type is not allowed for local processing";
}

void FileRequestSenderLocal::SendFilePacket(
    std::shared_ptr<FileReplyReceiverProxy> /* receiver */,
    std::unique_ptr<proto::file_transfer::FilePacket> /* file_packet */)
{
    DLOG(FATAL) << "The request type is not allowed for local processing";
}

void FileRequestSenderLocal::SendFilePacketRequest(
    std::shared_ptr<FileReplyReceiverProxy> /* receiver */)
{
    DLOG(FATAL) << "The request type is not allowed for local processing";
}

} // namespace aspia
