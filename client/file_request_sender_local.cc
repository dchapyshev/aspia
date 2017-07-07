//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/file_request_sender_local.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/file_request_sender_local.h"
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

void FileRequestSenderLocal::SendDriveListRequest(std::shared_ptr<FileReplyReceiverProxy> receiver)
{
    if (!worker_->BelongsToCurrentThread())
    {
        worker_->PostTask(std::bind(&FileRequestSenderLocal::SendDriveListRequest,
                                    this, receiver));
        return;
    }

    std::unique_ptr<proto::DriveList> drive_list =
        std::make_unique<proto::DriveList>();

    proto::Status status = ExecuteDriveListRequest(drive_list.get());

    if (status != proto::Status::STATUS_SUCCESS)
    {
        receiver->OnLastRequestFailed(status);
        return;
    }

    receiver->OnDriveListReply(std::move(drive_list));
}

void FileRequestSenderLocal::SendFileListRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                                 const FilePath& path)
{
    if (!worker_->BelongsToCurrentThread())
    {
        worker_->PostTask(std::bind(&FileRequestSenderLocal::SendFileListRequest,
                                    this, receiver, path));
        return;
    }

    std::unique_ptr<proto::FileList> file_list = std::make_unique<proto::FileList>();

    proto::Status status = ExecuteFileListRequest(path, file_list.get());

    if (status != proto::Status::STATUS_SUCCESS)
    {
        receiver->OnLastRequestFailed(status);
        return;
    }

    receiver->OnFileListReply(std::move(file_list));
}

void FileRequestSenderLocal::SendCreateDirectoryRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                                        const FilePath& path)
{
    if (!worker_->BelongsToCurrentThread())
    {
        worker_->PostTask(std::bind(&FileRequestSenderLocal::SendCreateDirectoryRequest,
                                    this, receiver, path));
        return;
    }

    proto::Status status = ExecuteCreateDirectoryRequest(path);

    if (status != proto::Status::STATUS_SUCCESS)
    {
        receiver->OnLastRequestFailed(status);
        return;
    }

    receiver->OnCreateDirectoryReply();
}

void FileRequestSenderLocal::SendDirectorySizeRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                                      const FilePath& path)
{
    if (!worker_->BelongsToCurrentThread())
    {
        worker_->PostTask(std::bind(&FileRequestSenderLocal::SendDirectorySizeRequest,
                                    this, receiver, path));
        return;
    }

    // TODO
}

void FileRequestSenderLocal::SendRemoveRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                               const FilePath& path)
{
    if (!worker_->BelongsToCurrentThread())
    {
        worker_->PostTask(std::bind(&FileRequestSenderLocal::SendRemoveRequest,
                                    this, receiver, path));
        return;
    }

    proto::Status status = ExecuteRemoveRequest(path);

    if (status != proto::Status::STATUS_SUCCESS)
    {
        receiver->OnLastRequestFailed(status);
        return;
    }

    receiver->OnRemoveReply();
}

void FileRequestSenderLocal::SendRenameRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                               const FilePath& old_name,
                                               const FilePath& new_name)
{
    if (!worker_->BelongsToCurrentThread())
    {
        worker_->PostTask(std::bind(&FileRequestSenderLocal::SendRenameRequest,
                                    this,
                                    receiver,
                                    old_name,
                                    new_name));
        return;
    }

    proto::Status status = ExecuteRenameRequest(old_name, new_name);

    if (status != proto::Status::STATUS_SUCCESS)
    {
        receiver->OnLastRequestFailed(status);
        return;
    }

    receiver->OnRenameReply();
}

} // namespace aspia
