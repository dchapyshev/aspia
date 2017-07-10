//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/file_transfer_session_client.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/file_transfer_session_client.h"
#include "base/files/file_helpers.h"
#include "base/strings/unicode.h"
#include "protocol/message_serialization.h"
#include "protocol/filesystem.h"
#include "proto/auth_session.pb.h"

namespace aspia {

namespace fs = std::experimental::filesystem;

void FileTransferSessionClient::Run(const std::wstring& input_channel_name,
                                    const std::wstring& output_channel_name)
{
    status_dialog_ = std::make_unique<UiFileStatusDialog>();

    ipc_channel_ = PipeChannel::CreateClient(input_channel_name,
                                             output_channel_name);
    if (ipc_channel_)
    {
        if (ipc_channel_->Connect(GetCurrentProcessId(), this))
        {
            status_dialog_->WaitForClose();
        }

        ipc_channel_.reset();
    }

    status_dialog_.reset();
}

void FileTransferSessionClient::OnPipeChannelConnect(uint32_t user_data)
{
    // The server sends the session type in user_data.
    proto::SessionType session_type = static_cast<proto::SessionType>(user_data);

    if (session_type != proto::SessionType::SESSION_TYPE_FILE_TRANSFER)
    {
        LOG(FATAL) << "Invalid session type passed: " << session_type;
        return;
    }

    status_dialog_->SetSessionStartedStatus();
}

void FileTransferSessionClient::OnPipeChannelDisconnect()
{
    status_dialog_->SetSessionTerminatedStatus();
}

void FileTransferSessionClient::OnPipeChannelMessage(const IOBuffer& buffer)
{
    proto::file_transfer::ClientToHost message;

    if (!ParseMessage(buffer, message))
    {
        ipc_channel_->Close();
        return;
    }

    switch (message.type())
    {
        case proto::RequestType::REQUEST_TYPE_DRIVE_LIST:
            ReadDriveListRequestMessage();
            break;

        case proto::RequestType::REQUEST_TYPE_FILE_LIST:
            ReadFileListRequestMessage(message.file_list_request());
            break;

        case proto::RequestType::REQUEST_TYPE_DIRECTORY_SIZE:
            // TODO
            break;

        case proto::RequestType::REQUEST_TYPE_CREATE_DIRECTORY:
            ReadCreateDirectoryRequest(message.create_directory_request());
            break;

        case proto::RequestType::REQUEST_TYPE_RENAME:
            ReadRenameRequest(message.rename_request());
            break;

        case proto::RequestType::REQUEST_TYPE_REMOVE:
            ReadRemoveRequest(message.remove_request());
            break;

        default:
            LOG(ERROR) << "Unknown message from client: " << message.type();
            ipc_channel_->Close();
            break;
    }
}

void FileTransferSessionClient::WriteMessage(
    const proto::file_transfer::HostToClient& message)
{
    IOBuffer buffer(SerializeMessage<IOBuffer>(message));
    std::lock_guard<std::mutex> lock(outgoing_lock_);
    ipc_channel_->Send(buffer);
}

void FileTransferSessionClient::ReadDriveListRequestMessage()
{
    proto::file_transfer::HostToClient message;
    message.set_type(proto::RequestType::REQUEST_TYPE_DRIVE_LIST);
    message.set_status(ExecuteDriveListRequest(message.mutable_drive_list()));

    status_dialog_->SetDriveListRequestStatus(message.status());
    WriteMessage(message);
}

void FileTransferSessionClient::ReadFileListRequestMessage(
    const proto::FileListRequest& request)
{
    proto::file_transfer::HostToClient message;

    FilePath path = fs::u8path(request.path());

    message.set_type(proto::RequestType::REQUEST_TYPE_FILE_LIST);
    message.set_status(ExecuteFileListRequest(path, message.mutable_file_list()));

    status_dialog_->SetFileListRequestStatus(path, message.status());
    WriteMessage(message);
}

void FileTransferSessionClient::ReadCreateDirectoryRequest(
    const proto::CreateDirectoryRequest& request)
{
    proto::file_transfer::HostToClient message;

    FilePath path = fs::u8path(request.path());

    message.set_type(proto::RequestType::REQUEST_TYPE_CREATE_DIRECTORY);
    message.set_status(ExecuteCreateDirectoryRequest(path));

    status_dialog_->SetCreateDirectoryRequestStatus(path, message.status());
    WriteMessage(message);
}

void FileTransferSessionClient::ReadRenameRequest(
    const proto::RenameRequest& request)
{
    proto::file_transfer::HostToClient message;

    FilePath old_name = fs::u8path(request.old_name());
    FilePath new_name = fs::u8path(request.new_name());

    message.set_type(proto::RequestType::REQUEST_TYPE_RENAME);
    message.set_status(ExecuteRenameRequest(old_name, new_name));

    status_dialog_->SetRenameRequestStatus(old_name, new_name, message.status());
    WriteMessage(message);
}

void FileTransferSessionClient::ReadRemoveRequest(
    const proto::RemoveRequest& request)
{
    proto::file_transfer::HostToClient message;

    FilePath path = fs::u8path(request.path());

    message.set_type(proto::RequestType::REQUEST_TYPE_REMOVE);
    message.set_status(ExecuteRemoveRequest(path));

    status_dialog_->SetRemoveRequestStatus(path, message.status());
    WriteMessage(message);
}

} // namespace aspia
