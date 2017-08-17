//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/file_request_sender_remote.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/file_request_sender_remote.h"
#include "protocol/message_serialization.h"
#include "base/logging.h"

namespace aspia {

FileRequestSenderRemote::FileRequestSenderRemote(
    std::shared_ptr<NetworkChannelProxy> channel_proxy)
    : channel_proxy_(channel_proxy)
{
    DCHECK(channel_proxy_);
}

void FileRequestSenderRemote::SendDriveListRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver)
{
    std::unique_ptr<proto::file_transfer::ClientToHost> request =
        std::make_unique<proto::file_transfer::ClientToHost>();

    request->mutable_drive_list_request()->set_dummy(1);

    SendRequest(receiver, std::move(request));
}

void FileRequestSenderRemote::SendFileListRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const FilePath& path)
{
    std::unique_ptr<proto::file_transfer::ClientToHost> request =
        std::make_unique<proto::file_transfer::ClientToHost>();

    request->mutable_file_list_request()->set_path(path.u8string());

    SendRequest(receiver, std::move(request));
}

void FileRequestSenderRemote::SendCreateDirectoryRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const FilePath& path)
{
    std::unique_ptr<proto::file_transfer::ClientToHost> request =
        std::make_unique<proto::file_transfer::ClientToHost>();

    request->mutable_create_directory_request()->set_path(path.u8string());

    SendRequest(receiver, std::move(request));
}

void FileRequestSenderRemote::SendDirectorySizeRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const FilePath& path)
{
    std::unique_ptr<proto::file_transfer::ClientToHost> request =
        std::make_unique<proto::file_transfer::ClientToHost>();

    request->mutable_directory_size_request()->set_path(path.u8string());

    SendRequest(receiver, std::move(request));
}

void FileRequestSenderRemote::SendRemoveRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                                const FilePath& path)
{
    std::unique_ptr<proto::file_transfer::ClientToHost> request =
        std::make_unique<proto::file_transfer::ClientToHost>();

    request->mutable_remove_request()->set_path(path.u8string());

    SendRequest(receiver, std::move(request));
}

void FileRequestSenderRemote::SendRenameRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                                const FilePath& old_name,
                                                const FilePath& new_name)
{
    std::unique_ptr<proto::file_transfer::ClientToHost> request =
        std::make_unique<proto::file_transfer::ClientToHost>();

    request->mutable_rename_request()->set_old_name(old_name.u8string());
    request->mutable_rename_request()->set_new_name(new_name.u8string());

    SendRequest(receiver, std::move(request));
}

void FileRequestSenderRemote::SendFileUploadRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const FilePath& file_path,
    Overwrite overwrite)
{
    std::unique_ptr<proto::file_transfer::ClientToHost> request =
        std::make_unique<proto::file_transfer::ClientToHost>();

    request->mutable_file_upload_request()->set_file_path(file_path.u8string());
    request->mutable_file_upload_request()->set_overwrite(
        overwrite == Overwrite::YES ? true : false);

    SendRequest(receiver, std::move(request));
}

void FileRequestSenderRemote::SendFilePacket(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                             std::unique_ptr<proto::FilePacket> file_packet)
{
    std::unique_ptr<proto::file_transfer::ClientToHost> request =
        std::make_unique<proto::file_transfer::ClientToHost>();

    request->set_allocated_file_packet(file_packet.release());

    SendRequest(receiver, std::move(request));
}

void FileRequestSenderRemote::SendRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    std::unique_ptr<proto::file_transfer::ClientToHost> request)
{
    std::lock_guard<std::mutex> lock(send_lock_);

    if (last_request_ || last_receiver_)
    {
        DLOG(ERROR) << "No response to previous request. New request ignored";
        return;
    }

    last_request_ = std::move(request);
    last_receiver_ = std::move(receiver);

    channel_proxy_->Send(SerializeMessage<IOBuffer>(*last_request_),
                         std::bind(&FileRequestSenderRemote::OnRequestSended, this));
}

void FileRequestSenderRemote::OnRequestSended()
{
    channel_proxy_->Receive(std::bind(
        &FileRequestSenderRemote::OnReplyReceived, this, std::placeholders::_1));
}

void FileRequestSenderRemote::OnReplyReceived(std::unique_ptr<IOBuffer> buffer)
{
    proto::file_transfer::HostToClient reply;

    if (!ParseMessage(*buffer, reply))
    {
        channel_proxy_->Disconnect();
        return;
    }

    if (!ProcessNextReply(reply))
    {
        channel_proxy_->Disconnect();
    }
}

bool FileRequestSenderRemote::ProcessNextReply(proto::file_transfer::HostToClient& reply)
{
    std::unique_ptr<proto::file_transfer::ClientToHost> request;
    std::shared_ptr<FileReplyReceiverProxy> receiver;

    {
        std::lock_guard<std::mutex> lock(send_lock_);

        DCHECK(last_request_);
        DCHECK(last_receiver_);

        request = std::move(last_request_);
        receiver = std::move(last_receiver_);
    }

    if (request->has_drive_list_request())
    {
        if (reply.status() == proto::REQUEST_STATUS_SUCCESS)
        {
            if (!reply.has_drive_list())
                return false;

            std::unique_ptr<proto::DriveList> drive_list(reply.release_drive_list());

            receiver->OnDriveListRequestReply(std::move(drive_list));
        }
        else
        {
            receiver->OnDriveListRequestFailure(reply.status());
        }
    }
    else if (request->has_file_list_request())
    {
        if (reply.status() == proto::REQUEST_STATUS_SUCCESS)
        {
            if (!reply.has_file_list())
                return false;

            std::unique_ptr<proto::FileList> file_list(reply.release_file_list());

            receiver->OnFileListRequestReply(
                std::experimental::filesystem::u8path(request->file_list_request().path()),
                std::move(file_list));
        }
        else
        {
            receiver->OnFileListRequestFailure(
                std::experimental::filesystem::u8path(request->file_list_request().path()),
                reply.status());
        }
    }
    else if (request->has_directory_size_request())
    {
        if (reply.status() == proto::REQUEST_STATUS_SUCCESS)
        {
            if (!reply.has_directory_size())
                return false;

            receiver->OnDirectorySizeRequestReply(
                std::experimental::filesystem::u8path(request->directory_size_request().path()),
                reply.directory_size().size());
        }
        else
        {
            receiver->OnDirectorySizeRequestFailure(
                std::experimental::filesystem::u8path(request->directory_size_request().path()),
                reply.status());
        }
    }
    else if (request->has_create_directory_request())
    {
        receiver->OnCreateDirectoryRequestReply(
            std::experimental::filesystem::u8path(request->create_directory_request().path()),
            reply.status());
    }
    else if (request->has_rename_request())
    {
        receiver->OnRenameRequestReply(
            std::experimental::filesystem::u8path(request->rename_request().old_name()),
            std::experimental::filesystem::u8path(request->rename_request().new_name()),
            reply.status());
    }
    else if (request->has_remove_request())
    {
        receiver->OnRemoveRequestReply(
            std::experimental::filesystem::u8path(request->remove_request().path()),
            reply.status());
    }
    else if (request->has_file_upload_request())
    {
        receiver->OnFileUploadRequestReply(
            std::experimental::filesystem::u8path(request->file_upload_request().file_path()),
            reply.status());
    }
    else if (request->has_file_packet())
    {
        receiver->OnFileUploadDataRequestReply(request->file_packet().flags(), reply.status());
    }
    else
    {
        DLOG(ERROR) << "Unhandled request type";
        return false;
    }

    return true;
}

} // namespace aspia
