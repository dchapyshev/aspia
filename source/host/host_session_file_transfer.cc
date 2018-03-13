//
// PROJECT:         Aspia
// FILE:            host/host_session_file_transfer.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_session_file_transfer.h"

#include <QDebug>

#include "base/files/file_helpers.h"
#include "base/files/file_util.h"
#include "base/process/process.h"
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

        uint32_t user_data = Process::Current().Pid();

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
    proto::auth::SessionType session_type =
        static_cast<proto::auth::SessionType>(user_data);

    if (session_type != proto::auth::SESSION_TYPE_FILE_TRANSFER)
    {
        qFatal("Invalid session type passed");
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

void HostSessionFileTransfer::OnIpcChannelMessage(const QByteArray& buffer)
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
        qWarning("Unknown message from client");
        ipc_channel_proxy_->Disconnect();
    }
}

void HostSessionFileTransfer::SendReply(const proto::file_transfer::HostToClient& reply)
{
    ipc_channel_proxy_->Send(
        SerializeMessage(reply),
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
    reply.set_status(FileSystemRequest::GetDriveList(reply.mutable_drive_list()));

    if (reply.status() == proto::file_transfer::STATUS_SUCCESS)
    {
        status_dialog_->OnDriveListRequest();
    }

    SendReply(reply);
}

void HostSessionFileTransfer::ReadFileListRequest(
    const proto::file_transfer::FileListRequest& request)
{
    proto::file_transfer::HostToClient reply;

    std::experimental::filesystem::path path =
        std::experimental::filesystem::u8path(request.path());

    reply.set_status(FileSystemRequest::GetFileList(path, reply.mutable_file_list()));

    if (reply.status() == proto::file_transfer::STATUS_SUCCESS)
    {
        status_dialog_->OnFileListRequest(path);
    }

    SendReply(reply);
}

void HostSessionFileTransfer::ReadCreateDirectoryRequest(
    const proto::file_transfer::CreateDirectoryRequest& request)
{
    proto::file_transfer::HostToClient reply;

    std::experimental::filesystem::path path =
        std::experimental::filesystem::u8path(request.path());

    reply.set_status(FileSystemRequest::CreateDirectory(path));

    if (reply.status() == proto::file_transfer::STATUS_SUCCESS)
    {
        status_dialog_->OnCreateDirectoryRequest(path);
    }

    SendReply(reply);
}

void HostSessionFileTransfer::ReadDirectorySizeRequest(
    const proto::file_transfer::DirectorySizeRequest& request)
{
    proto::file_transfer::HostToClient reply;

    std::experimental::filesystem::path path =
        std::experimental::filesystem::u8path(request.path());

    uint64_t directory_size = 0;

    reply.set_status(FileSystemRequest::GetDirectorySize(path, directory_size));
    reply.mutable_directory_size()->set_size(directory_size);

    SendReply(reply);
}

void HostSessionFileTransfer::ReadRenameRequest(const proto::file_transfer::RenameRequest& request)
{
    proto::file_transfer::HostToClient reply;

    std::experimental::filesystem::path old_name =
        std::experimental::filesystem::u8path(request.old_name());
    std::experimental::filesystem::path new_name =
        std::experimental::filesystem::u8path(request.new_name());

    reply.set_status(FileSystemRequest::Rename(old_name, new_name));

    if (reply.status() == proto::file_transfer::STATUS_SUCCESS)
    {
        status_dialog_->OnRenameRequest(old_name, new_name);
    }

    SendReply(reply);
}

void HostSessionFileTransfer::ReadRemoveRequest(const proto::file_transfer::RemoveRequest& request)
{
    proto::file_transfer::HostToClient reply;

    std::experimental::filesystem::path path =
        std::experimental::filesystem::u8path(request.path());

    reply.set_status(FileSystemRequest::Remove(path));

    if (reply.status() == proto::file_transfer::STATUS_SUCCESS)
    {
        status_dialog_->OnRemoveRequest(path);
    }

    SendReply(reply);
}

void HostSessionFileTransfer::ReadFileUploadRequest(
    const proto::file_transfer::FileUploadRequest& request)
{
    proto::file_transfer::HostToClient reply;

    std::experimental::filesystem::path file_path =
        std::experimental::filesystem::u8path(request.file_path());

    do
    {
        if (!IsValidPathName(file_path))
        {
            reply.set_status(proto::file_transfer::STATUS_INVALID_PATH_NAME);
            break;
        }

        if (!request.overwrite())
        {
            if (PathExists(file_path))
            {
                reply.set_status(proto::file_transfer::STATUS_PATH_ALREADY_EXISTS);
                break;
            }
        }

        file_depacketizer_ = FileDepacketizer::Create(file_path, request.overwrite());
        if (!file_depacketizer_)
        {
            reply.set_status(proto::file_transfer::STATUS_FILE_CREATE_ERROR);
            break;
        }

        reply.set_status(proto::file_transfer::STATUS_SUCCESS);
    }
    while (false);

    if (reply.status() == proto::file_transfer::STATUS_SUCCESS)
    {
        status_dialog_->OnFileUploadRequest(file_path);
    }

    SendReply(reply);
}

bool HostSessionFileTransfer::ReadFilePacket(const proto::file_transfer::FilePacket& file_packet)
{
    if (!file_depacketizer_)
    {
        qWarning("Unexpected file packet");
        return false;
    }

    proto::file_transfer::HostToClient reply;

    if (!file_depacketizer_->ReadNextPacket(file_packet))
    {
        reply.set_status(proto::file_transfer::STATUS_FILE_WRITE_ERROR);
    }
    else
    {
        reply.set_status(proto::file_transfer::STATUS_SUCCESS);
    }

    if (file_packet.flags() & proto::file_transfer::FilePacket::FLAG_LAST_PACKET)
    {
        file_depacketizer_.reset();
    }

    SendReply(reply);
    return true;
}

void HostSessionFileTransfer::ReadFileDownloadRequest(
    const proto::file_transfer::FileDownloadRequest& request)
{
    proto::file_transfer::HostToClient reply;

    std::experimental::filesystem::path file_path =
        std::experimental::filesystem::u8path(request.file_path());

    if (!IsValidPathName(file_path))
    {
        reply.set_status(proto::file_transfer::STATUS_INVALID_PATH_NAME);
    }
    else
    {
        file_packetizer_ = FilePacketizer::Create(file_path);
        if (!file_packetizer_)
        {
            reply.set_status(proto::file_transfer::STATUS_FILE_OPEN_ERROR);
        }
        else
        {
            reply.set_status(proto::file_transfer::STATUS_SUCCESS);
        }
    }

    if (reply.status() == proto::file_transfer::STATUS_SUCCESS)
    {
        status_dialog_->OnFileDownloadRequest(file_path);
    }

    SendReply(reply);
}

bool HostSessionFileTransfer::ReadFilePacketRequest()
{
    if (!file_packetizer_)
    {
        qWarning("Unexpected download data request");
        return false;
    }

    proto::file_transfer::HostToClient reply;

    std::unique_ptr<proto::file_transfer::FilePacket> packet =
        file_packetizer_->CreateNextPacket();
    if (!packet)
    {
        reply.set_status(proto::file_transfer::STATUS_FILE_READ_ERROR);
    }
    else
    {
        if (packet->flags() & proto::file_transfer::FilePacket::FLAG_LAST_PACKET)
        {
            file_packetizer_.reset();
        }

        reply.set_status(proto::file_transfer::STATUS_SUCCESS);
        reply.set_allocated_file_packet(packet.release());
    }

    SendReply(reply);
    return true;
}

} // namespace aspia
