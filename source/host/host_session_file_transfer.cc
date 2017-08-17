//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_session_file_transfer.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_session_file_transfer.h"
#include "base/files/file_helpers.h"
#include "ipc/pipe_channel_proxy.h"
#include "protocol/message_serialization.h"
#include "protocol/filesystem.h"
#include "proto/auth_session.pb.h"

namespace aspia {

void HostSessionFileTransfer::Run(const std::wstring& channel_id)
{
    status_dialog_ = std::make_unique<FileStatusDialog>();

    ipc_channel_ = PipeChannel::CreateClient(channel_id);
    if (ipc_channel_)
    {
        ipc_channel_proxy_ = ipc_channel_->pipe_channel_proxy();

        uint32_t user_data = GetCurrentProcessId();

        PipeChannel::DisconnectHandler disconnect_handler =
            std::bind(&HostSessionFileTransfer::OnIpcChannelDisconnect, this);

        if (ipc_channel_->Connect(user_data, std::move(disconnect_handler)))
        {
            OnIpcChannelConnect(user_data);
            status_dialog_->WaitForClose();
        }

        ipc_channel_.reset();
    }

    status_dialog_.reset();
}

void HostSessionFileTransfer::OnIpcChannelConnect(uint32_t user_data)
{
    // The server sends the session type in user_data.
    proto::SessionType session_type =
        static_cast<proto::SessionType>(user_data);

    if (session_type != proto::SESSION_TYPE_FILE_TRANSFER)
    {
        LOG(FATAL) << "Invalid session type passed: " << session_type;
        return;
    }

    status_dialog_->OnSessionStarted();

    ipc_channel_proxy_->Receive(std::bind(
        &HostSessionFileTransfer::OnIpcChannelMessage, this, std::placeholders::_1));
}

void HostSessionFileTransfer::OnIpcChannelDisconnect()
{
    status_dialog_->OnSessionTerminated();
}

void HostSessionFileTransfer::OnIpcChannelMessage(const IOBuffer& buffer)
{
    proto::file_transfer::ClientToHost message;

    if (!ParseMessage(buffer, message))
    {
        ipc_channel_proxy_->Disconnect();
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
        if (!ReadFilePacket(message.file_packet()))
            ipc_channel_proxy_->Disconnect();
    }
    else if (message.has_file_download_request())
    {
        ReadFileDownloadRequest(message.file_download_request());
    }
    else if (message.has_file_packet_request())
    {
        if (!ReadFilePacketRequest())
            ipc_channel_proxy_->Disconnect();
    }
    else
    {
        LOG(ERROR) << "Unknown message from client";
        ipc_channel_proxy_->Disconnect();
    }
}

void HostSessionFileTransfer::SendReply(const proto::file_transfer::HostToClient& reply)
{
    ipc_channel_proxy_->Send(
        SerializeMessage<IOBuffer>(reply),
        std::bind(&HostSessionFileTransfer::OnReplySended, this));
}

void HostSessionFileTransfer::OnReplySended()
{
    // Receive next request.
    ipc_channel_proxy_->Receive(std::bind(
        &HostSessionFileTransfer::OnIpcChannelMessage, this, std::placeholders::_1));
}

void HostSessionFileTransfer::ReadDriveListRequest()
{
    proto::file_transfer::HostToClient reply;
    reply.set_status(ExecuteDriveListRequest(reply.mutable_drive_list()));

    if (reply.status() == proto::REQUEST_STATUS_SUCCESS)
    {
        status_dialog_->OnDriveListRequest();
    }

    SendReply(reply);
}

void HostSessionFileTransfer::ReadFileListRequest(const proto::FileListRequest& request)
{
    proto::file_transfer::HostToClient reply;

    FilePath path = std::experimental::filesystem::u8path(request.path());

    reply.set_status(ExecuteFileListRequest(path, reply.mutable_file_list()));

    if (reply.status() == proto::REQUEST_STATUS_SUCCESS)
    {
        status_dialog_->OnFileListRequest(path);
    }

    SendReply(reply);
}

void HostSessionFileTransfer::ReadCreateDirectoryRequest(
    const proto::CreateDirectoryRequest& request)
{
    proto::file_transfer::HostToClient reply;

    FilePath path = std::experimental::filesystem::u8path(request.path());

    reply.set_status(ExecuteCreateDirectoryRequest(path));

    if (reply.status() == proto::REQUEST_STATUS_SUCCESS)
    {
        status_dialog_->OnCreateDirectoryRequest(path);
    }

    SendReply(reply);
}

void HostSessionFileTransfer::ReadDirectorySizeRequest(const proto::DirectorySizeRequest& request)
{
    proto::file_transfer::HostToClient reply;

    FilePath path = std::experimental::filesystem::u8path(request.path());

    uint64_t directory_size = 0;

    reply.set_status(ExecuteDirectorySizeRequest(path, directory_size));
    reply.mutable_directory_size()->set_size(directory_size);

    SendReply(reply);
}

void HostSessionFileTransfer::ReadRenameRequest(const proto::RenameRequest& request)
{
    proto::file_transfer::HostToClient reply;

    FilePath old_name = std::experimental::filesystem::u8path(request.old_name());
    FilePath new_name = std::experimental::filesystem::u8path(request.new_name());

    reply.set_status(ExecuteRenameRequest(old_name, new_name));

    if (reply.status() == proto::REQUEST_STATUS_SUCCESS)
    {
        status_dialog_->OnRenameRequest(old_name, new_name);
    }

    SendReply(reply);
}

void HostSessionFileTransfer::ReadRemoveRequest(const proto::RemoveRequest& request)
{
    proto::file_transfer::HostToClient reply;

    FilePath path = std::experimental::filesystem::u8path(request.path());

    reply.set_status(ExecuteRemoveRequest(path));

    if (reply.status() == proto::REQUEST_STATUS_SUCCESS)
    {
        status_dialog_->OnRemoveRequest(path);
    }

    SendReply(reply);
}

void HostSessionFileTransfer::ReadFileUploadRequest(const proto::FileUploadRequest& request)
{
    proto::file_transfer::HostToClient reply;

    FilePath file_path = std::experimental::filesystem::u8path(request.file_path());

    do
    {
        if (!IsValidPathName(file_path))
        {
            reply.set_status(proto::REQUEST_STATUS_INVALID_PATH_NAME);
            break;
        }

        if (!request.overwrite())
        {
            std::error_code code;

            if (std::experimental::filesystem::exists(file_path, code))
            {
                reply.set_status(proto::REQUEST_STATUS_PATH_ALREADY_EXISTS);
                break;
            }
        }

        file_depacketizer_ = FileDepacketizer::Create(file_path, request.overwrite());
        if (!file_depacketizer_)
        {
            reply.set_status(proto::REQUEST_STATUS_FILE_CREATE_ERROR);
            break;
        }

        reply.set_status(proto::REQUEST_STATUS_SUCCESS);
    }
    while (false);

    if (reply.status() == proto::REQUEST_STATUS_SUCCESS)
    {
        status_dialog_->OnFileUploadRequest(file_path);
    }

    SendReply(reply);
}

bool HostSessionFileTransfer::ReadFilePacket(const proto::FilePacket& file_packet)
{
    if (!file_depacketizer_)
    {
        LOG(ERROR) << "Unexpected file packet";
        return false;
    }

    proto::file_transfer::HostToClient reply;

    if (!file_depacketizer_->ReadNextPacket(file_packet))
    {
        reply.set_status(proto::REQUEST_STATUS_FILE_WRITE_ERROR);
    }
    else
    {
        reply.set_status(proto::REQUEST_STATUS_SUCCESS);
    }

    if (file_packet.flags() & proto::FilePacket::LAST_PACKET)
    {
        file_depacketizer_.reset();
    }

    SendReply(reply);
    return true;
}

void HostSessionFileTransfer::ReadFileDownloadRequest(const proto::FileDownloadRequest& request)
{
    proto::file_transfer::HostToClient reply;

    FilePath file_path = std::experimental::filesystem::u8path(request.file_path());

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
        else
        {
            reply.set_status(proto::REQUEST_STATUS_SUCCESS);
        }
    }

    SendReply(reply);
}

bool HostSessionFileTransfer::ReadFilePacketRequest()
{
    if (!file_packetizer_)
    {
        LOG(ERROR) << "Unexpected download data request";
        return false;
    }

    proto::file_transfer::HostToClient reply;

    std::unique_ptr<proto::FilePacket> packet = file_packetizer_->CreateNextPacket();
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

        reply.set_status(proto::REQUEST_STATUS_SUCCESS);
        reply.set_allocated_file_packet(packet.release());
    }

    SendReply(reply);
    return true;
}

} // namespace aspia
