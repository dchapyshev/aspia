//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/file_transfer_session_client.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/file_transfer_session_client.h"
#include "base/files/file_helpers.h"
#include "ipc/pipe_channel_proxy.h"
#include "protocol/message_serialization.h"
#include "protocol/filesystem.h"
#include "proto/auth_session.pb.h"

namespace aspia {

void FileTransferSessionClient::Run(const std::wstring& channel_id)
{
    status_dialog_ = std::make_unique<UiFileStatusDialog>();

    ipc_channel_ = PipeChannel::CreateClient(channel_id);
    if (ipc_channel_)
    {
        ipc_channel_proxy_ = ipc_channel_->pipe_channel_proxy();

        uint32_t user_data = GetCurrentProcessId();

        PipeChannel::DisconnectHandler disconnect_handler =
            std::bind(&FileTransferSessionClient::OnIpcChannelDisconnect, this);

        if (ipc_channel_->Connect(user_data, std::move(disconnect_handler)))
        {
            OnIpcChannelConnect(user_data);
            status_dialog_->WaitForClose();
        }

        ipc_channel_.reset();
    }

    status_dialog_.reset();
}

void FileTransferSessionClient::OnIpcChannelConnect(uint32_t user_data)
{
    // The server sends the session type in user_data.
    proto::SessionType session_type =
        static_cast<proto::SessionType>(user_data);

    if (session_type != proto::SESSION_TYPE_FILE_TRANSFER)
    {
        LOG(FATAL) << "Invalid session type passed: " << session_type;
        return;
    }

    status_dialog_->SetSessionStartedStatus();

    ipc_channel_proxy_->Receive(std::bind(
        &FileTransferSessionClient::OnIpcChannelMessage, this,
        std::placeholders::_1));
}

void FileTransferSessionClient::OnIpcChannelDisconnect()
{
    status_dialog_->SetSessionTerminatedStatus();
}

void FileTransferSessionClient::OnIpcChannelMessage(
    std::unique_ptr<IOBuffer> buffer)
{
    proto::file_transfer::ClientToHost message;

    if (!ParseMessage(*buffer, message))
    {
        ipc_channel_.reset();
        return;
    }

    if (message.has_drive_list_request())
    {
        ReadDriveListRequest();
    }
    else if (message.has_file_list_request())
    {
        ReadFileListRequest(message.file_list_request());
    }
    else if (message.has_directory_size_request())
    {
        ReadDirectorySizeRequest(message.directory_size_request());
    }
    else if (message.has_create_directory_request())
    {
        ReadCreateDirectoryRequest(message.create_directory_request());
    }
    else if (message.has_rename_request())
    {
        ReadRenameRequest(message.rename_request());
    }
    else if (message.has_remove_request())
    {
        ReadRemoveRequest(message.remove_request());
    }
    else if (message.has_file_upload_request())
    {
        ReadFileUploadRequest(message.file_upload_request());
    }
    else if (message.has_file_packet())
    {
        if (!ReadFileUploadDataRequest(message.file_packet()))
            ipc_channel_.reset();
    }
    else if (message.has_file_download_request())
    {
        ReadFileDownloadRequest(message.file_download_request());
    }
    else if (message.has_file_packet_request())
    {
        if (!ReadFilePacketRequest())
            ipc_channel_.reset();
    }
    else
    {
        LOG(ERROR) << "Unknown message from client";
        ipc_channel_.reset();
    }
}

void FileTransferSessionClient::SendReply(
    const proto::file_transfer::HostToClient& reply)
{
    ipc_channel_proxy_->Send(
        SerializeMessage<IOBuffer>(reply),
        std::bind(&FileTransferSessionClient::OnReplySended, this));
}

void FileTransferSessionClient::OnReplySended()
{
    // Receive next request.
    ipc_channel_proxy_->Receive(std::bind(
        &FileTransferSessionClient::OnIpcChannelMessage, this,
        std::placeholders::_1));
}

void FileTransferSessionClient::ReadDriveListRequest()
{
    proto::file_transfer::HostToClient reply;
    reply.set_status(ExecuteDriveListRequest(reply.mutable_drive_list()));

    status_dialog_->SetDriveListRequestStatus(reply.status());
    SendReply(reply);
}

void FileTransferSessionClient::ReadFileListRequest(
    const proto::FileListRequest& request)
{
    proto::file_transfer::HostToClient reply;

    FilePath path = std::experimental::filesystem::u8path(request.path());

    reply.set_status(ExecuteFileListRequest(path, reply.mutable_file_list()));

    status_dialog_->SetFileListRequestStatus(path, reply.status());
    SendReply(reply);
}

void FileTransferSessionClient::ReadCreateDirectoryRequest(
    const proto::CreateDirectoryRequest& request)
{
    proto::file_transfer::HostToClient reply;

    FilePath path = std::experimental::filesystem::u8path(request.path());

    reply.set_status(ExecuteCreateDirectoryRequest(path));

    status_dialog_->SetCreateDirectoryRequestStatus(path, reply.status());
    SendReply(reply);
}

void FileTransferSessionClient::ReadDirectorySizeRequest(
    const proto::DirectorySizeRequest& request)
{
    proto::file_transfer::HostToClient reply;

    FilePath path = std::experimental::filesystem::u8path(request.path());

    uint64_t directory_size = 0;

    reply.set_status(ExecuteDirectorySizeRequest(path, directory_size));
    reply.mutable_directory_size()->set_size(directory_size);

    SendReply(reply);
}

void FileTransferSessionClient::ReadRenameRequest(
    const proto::RenameRequest& request)
{
    proto::file_transfer::HostToClient reply;

    FilePath old_name =
        std::experimental::filesystem::u8path(request.old_name());
    FilePath new_name =
        std::experimental::filesystem::u8path(request.new_name());

    reply.set_status(ExecuteRenameRequest(old_name, new_name));

    status_dialog_->SetRenameRequestStatus(old_name, new_name, reply.status());
    SendReply(reply);
}

void FileTransferSessionClient::ReadRemoveRequest(
    const proto::RemoveRequest& request)
{
    proto::file_transfer::HostToClient reply;

    FilePath path = std::experimental::filesystem::u8path(request.path());

    reply.set_status(ExecuteRemoveRequest(path));

    status_dialog_->SetRemoveRequestStatus(path, reply.status());
    SendReply(reply);
}

void FileTransferSessionClient::ReadFileUploadRequest(
    const proto::FileUploadRequest& request)
{
    proto::file_transfer::HostToClient reply;

    FilePath file_path =
        std::experimental::filesystem::u8path(request.file_path());

    if (!IsValidPathName(file_path))
    {
        reply.set_status(proto::REQUEST_STATUS_INVALID_PATH_NAME);
    }
    else
    {
        std::error_code code;

        if (std::experimental::filesystem::exists(file_path, code))
        {
            reply.set_status(proto::REQUEST_STATUS_PATH_ALREADY_EXISTS);
        }
        else
        {
            file_depacketizer_ = FileDepacketizer::Create(file_path);
            if (!file_depacketizer_)
            {
                reply.set_status(proto::REQUEST_STATUS_FILE_CREATE_ERROR);
            }
        }
    }

    status_dialog_->SetFileUploadRequestStatus(file_path, reply.status());

    SendReply(reply);
}

bool FileTransferSessionClient::ReadFileUploadDataRequest(
    const proto::FilePacket& file_packet)
{
    if (!file_depacketizer_)
    {
        LOG(ERROR) << "Unexpected upload data request";
        return false;
    }

    proto::file_transfer::HostToClient reply;

    if (!file_depacketizer_->ReadNextPacket(file_packet))
    {
        reply.set_status(proto::REQUEST_STATUS_FILE_WRITE_ERROR);
    }

    if (file_packet.flags() & proto::FilePacket::LAST_PACKET)
    {
        file_depacketizer_.reset();
    }

    SendReply(reply);
    return true;
}

void FileTransferSessionClient::ReadFileDownloadRequest(
    const proto::FileDownloadRequest& request)
{
    proto::file_transfer::HostToClient reply;

    FilePath file_path =
        std::experimental::filesystem::u8path(request.file_path());

    if (!IsValidPathName(file_path))
    {
        reply.set_status(proto::REQUEST_STATUS_INVALID_PATH_NAME);
    }
    else
    {
        file_packetizer_ = FilePacketizer::Create(file_path);
        if (!file_packetizer_)
        {
            reply.set_status(proto::REQUEST_STATUS_FILE_OPEN_ERROR);
        }
    }

    SendReply(reply);
}

bool FileTransferSessionClient::ReadFilePacketRequest()
{
    if (!file_packetizer_)
    {
        LOG(ERROR) << "Unexpected download data request";
        return false;
    }

    proto::file_transfer::HostToClient reply;

    std::unique_ptr<proto::FilePacket> packet =
        file_packetizer_->CreateNextPacket();

    if (!packet)
    {
        reply.set_status(proto::REQUEST_STATUS_FILE_READ_ERROR);
    }
    else
    {
        if (packet->flags() & proto::FilePacket::LAST_PACKET)
        {
            file_packetizer_.reset();
        }

        reply.set_allocated_file_packet(packet.release());
    }

    SendReply(reply);
    return true;
}

} // namespace aspia
