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

void FileRequestSenderRemote::SendRemoveRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
    const FilePath& path)
{
    std::unique_ptr<proto::file_transfer::ClientToHost> request =
        std::make_unique<proto::file_transfer::ClientToHost>();

    request->mutable_remove_request()->set_path(path.u8string());

    SendRequest(receiver, std::move(request));
}

void FileRequestSenderRemote::SendRenameRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
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
    const FilePath& file_path)
{
    std::unique_ptr<proto::file_transfer::ClientToHost> request =
        std::make_unique<proto::file_transfer::ClientToHost>();

    request->mutable_file_upload_request()->set_file_path(file_path.u8string());
    request->mutable_file_upload_request()->set_overwrite(false);

    SendRequest(receiver, std::move(request));
}

void FileRequestSenderRemote::SendFileUploadDataRequest(
    std::shared_ptr<FileReplyReceiverProxy> receiver,
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
    channel_proxy_->Send(
        SerializeMessage<IOBuffer>(*request),
        std::bind(&FileRequestSenderRemote::OnRequestSended, this));
    receiver_queue_.Add(std::move(request), receiver);
}

void FileRequestSenderRemote::OnRequestSended()
{
    channel_proxy_->Receive(std::bind(
        &FileRequestSenderRemote::OnReplyReceived, this,
        std::placeholders::_1));
}

void FileRequestSenderRemote::OnReplyReceived(std::unique_ptr<IOBuffer> buffer)
{
    proto::file_transfer::HostToClient reply;

    if (!ParseMessage(*buffer, reply))
    {
        channel_proxy_->Disconnect();
        return;
    }

    if (!receiver_queue_.ProcessNextReply(reply))
    {
        channel_proxy_->Disconnect();
    }
}

} // namespace aspia
