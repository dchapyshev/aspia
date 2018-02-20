//
// PROJECT:         Aspia
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
    proto::file_transfer::ClientToHost request;
    request.mutable_drive_list_request()->set_dummy(1);
    SendRequest(receiver, std::move(request));
}

void FileRequestSenderRemote::SendFileListRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const std::experimental::filesystem::path& path)
{
    proto::file_transfer::ClientToHost request;
    request.mutable_file_list_request()->set_path(path.u8string());
    SendRequest(receiver, std::move(request));
}

void FileRequestSenderRemote::SendCreateDirectoryRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const std::experimental::filesystem::path& path)
{
    proto::file_transfer::ClientToHost request;
    request.mutable_create_directory_request()->set_path(path.u8string());
    SendRequest(receiver, std::move(request));
}

void FileRequestSenderRemote::SendDirectorySizeRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const std::experimental::filesystem::path& path)
{
    proto::file_transfer::ClientToHost request;
    request.mutable_directory_size_request()->set_path(path.u8string());
    SendRequest(receiver, std::move(request));
}

void FileRequestSenderRemote::SendRemoveRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const std::experimental::filesystem::path& path)
{
    proto::file_transfer::ClientToHost request;
    request.mutable_remove_request()->set_path(path.u8string());
    SendRequest(receiver, std::move(request));
}

void FileRequestSenderRemote::SendRenameRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const std::experimental::filesystem::path& old_name,
    const std::experimental::filesystem::path& new_name)
{
    proto::file_transfer::ClientToHost request;
    request.mutable_rename_request()->set_old_name(old_name.u8string());
    request.mutable_rename_request()->set_new_name(new_name.u8string());
    SendRequest(receiver, std::move(request));
}

void FileRequestSenderRemote::SendFileUploadRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const std::experimental::filesystem::path& file_path,
    Overwrite overwrite)
{
    proto::file_transfer::ClientToHost request;

    request.mutable_file_upload_request()->set_file_path(file_path.u8string());
    request.mutable_file_upload_request()->set_overwrite(
        overwrite == Overwrite::YES ? true : false);

    SendRequest(receiver, std::move(request));
}

void FileRequestSenderRemote::SendFileDownloadRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const std::experimental::filesystem::path& file_path)
{
    proto::file_transfer::ClientToHost request;
    request.mutable_file_download_request()->set_file_path(file_path.u8string());
    SendRequest(receiver, std::move(request));
}

void FileRequestSenderRemote::SendFilePacket(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    std::unique_ptr<proto::file_transfer::FilePacket> file_packet)
{
    proto::file_transfer::ClientToHost request;
    request.set_allocated_file_packet(file_packet.release());
    SendRequest(receiver, std::move(request));
}

void FileRequestSenderRemote::SendFilePacketRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver)
{
    proto::file_transfer::ClientToHost request;
    request.mutable_file_packet_request()->set_dummy(1);
    SendRequest(receiver, std::move(request));
}

void FileRequestSenderRemote::SendRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    proto::file_transfer::ClientToHost&& request)
{
    std::scoped_lock<std::mutex> lock(send_lock_);

    if (last_receiver_)
    {
        DLOG(LS_ERROR) << "No response to previous request. New request ignored";
        return;
    }

    last_request_ = std::move(request);
    last_receiver_ = std::move(receiver);

    channel_proxy_->Send(SerializeMessage(last_request_),
                         std::bind(&FileRequestSenderRemote::OnRequestSended, this));
}

void FileRequestSenderRemote::OnRequestSended()
{
    channel_proxy_->Receive(std::bind(
        &FileRequestSenderRemote::OnReplyReceived, this, std::placeholders::_1));
}

void FileRequestSenderRemote::OnReplyReceived(const IOBuffer& buffer)
{
    proto::file_transfer::HostToClient reply;

    if (!ParseMessage(buffer, reply))
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
    proto::file_transfer::ClientToHost request;
    std::shared_ptr<FileReplyReceiverProxy> receiver;

    {
        std::scoped_lock<std::mutex> lock(send_lock_);

        DCHECK(last_receiver_);

        request = std::move(last_request_);
        receiver = std::move(last_receiver_);
    }

    if (request.has_drive_list_request())
    {
        if (reply.status() == proto::file_transfer::STATUS_SUCCESS && !reply.has_drive_list())
            return false;

        std::unique_ptr<proto::file_transfer::DriveList> drive_list(reply.release_drive_list());
        receiver->OnDriveListReply(std::move(drive_list), reply.status());
    }
    else if (request.has_file_list_request())
    {
        if (reply.status() == proto::file_transfer::STATUS_SUCCESS && !reply.has_file_list())
            return false;

        std::unique_ptr<proto::file_transfer::FileList> file_list(reply.release_file_list());

        receiver->OnFileListReply(
            std::experimental::filesystem::u8path(request.file_list_request().path()),
            std::move(file_list),
            reply.status());
    }
    else if (request.has_directory_size_request())
    {
        if (reply.status() == proto::file_transfer::STATUS_SUCCESS && !reply.has_directory_size())
            return false;

        receiver->OnDirectorySizeReply(
            std::experimental::filesystem::u8path(request.directory_size_request().path()),
            reply.directory_size().size(),
            reply.status());
    }
    else if (request.has_create_directory_request())
    {
        receiver->OnCreateDirectoryReply(
            std::experimental::filesystem::u8path(request.create_directory_request().path()),
            reply.status());
    }
    else if (request.has_rename_request())
    {
        receiver->OnRenameReply(
            std::experimental::filesystem::u8path(request.rename_request().old_name()),
            std::experimental::filesystem::u8path(request.rename_request().new_name()),
            reply.status());
    }
    else if (request.has_remove_request())
    {
        receiver->OnRemoveReply(
            std::experimental::filesystem::u8path(request.remove_request().path()),
            reply.status());
    }
    else if (request.has_file_upload_request())
    {
        receiver->OnFileUploadReply(
            std::experimental::filesystem::u8path(request.file_upload_request().file_path()),
            reply.status());
    }
    else if (request.has_file_download_request())
    {
        receiver->OnFileDownloadReply(
            std::experimental::filesystem::u8path(request.file_download_request().file_path()),
            reply.status());
    }
    else if (request.has_file_packet_request())
    {
        if (reply.status() == proto::file_transfer::STATUS_SUCCESS && !reply.has_file_packet())
            return false;

        std::unique_ptr<proto::file_transfer::FilePacket> file_packet(reply.release_file_packet());
        receiver->OnFilePacketReceived(std::move(file_packet), reply.status());
    }
    else if (request.has_file_packet())
    {
        receiver->OnFilePacketSended(request.file_packet().flags(), reply.status());
    }
    else
    {
        DLOG(LS_ERROR) << "Unhandled request type";
        return false;
    }

    return true;
}

} // namespace aspia
