//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/file_transfer_session_client.cc
// LICENSE:         See top-level directory
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
    ipc_channel_ = PipeChannel::CreateClient(input_channel_name,
                                             output_channel_name);
    if (!ipc_channel_)
        return;

    if (ipc_channel_->Connect(GetCurrentProcessId(), this))
    {
        // Waiting for the connection to close.
        ipc_channel_->Wait();
    }

    ipc_channel_.reset();
}

void FileTransferSessionClient::OnWindowClose()
{
    ipc_channel_->Close();
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

    status_dialog_.reset(new UiFileStatusDialog(this));
}

void FileTransferSessionClient::OnPipeChannelDisconnect()
{
    // Nothing
}

void FileTransferSessionClient::OnPipeChannelMessage(const IOBuffer& buffer)
{
    proto::file_transfer::ClientToHost message;

    if (ParseMessage(buffer, message))
    {
        bool success = true;

        if (message.has_drive_list_request())
        {
            success = ReadDriveListRequestMessage(message.drive_list_request());
        }
        else if (message.has_file_list_request())
        {
            ReadFileListRequestMessage(message.file_list_request());
        }
        else if (message.has_file_request())
        {
            //success = ReadFileRequestMessage(message.file_request());
        }
        else if (message.has_file_packet())
        {
            //success = ReadFilePacketMessage(message.file_packet());
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
        else
        {
            // Unknown messages are ignored.
            DLOG(WARNING) << "Unhandled message from host";
        }

        if (success)
            return;
    }

    ipc_channel_->Close();
}

void FileTransferSessionClient::WriteMessage(
    const proto::file_transfer::HostToClient& message)
{
    IOBuffer buffer(SerializeMessage<IOBuffer>(message));
    std::lock_guard<std::mutex> lock(outgoing_lock_);
    ipc_channel_->Send(buffer);
}

void FileTransferSessionClient::WriteStatus(proto::Status status)
{
    proto::file_transfer::HostToClient message;
    message.set_status(status);
    WriteMessage(message);
}

bool FileTransferSessionClient::ReadDriveListRequestMessage(
    const proto::DriveListRequest& request)
{
    proto::file_transfer::HostToClient message;
    message.set_status(ExecuteDriveListRequest(message.mutable_drive_list()));

    status_dialog_->SetDriveListRequestStatus(message.status());
    WriteMessage(message);

    return true;
}

void FileTransferSessionClient::ReadFileListRequestMessage(
    const proto::FileListRequest& request)
{
    proto::file_transfer::HostToClient message;

    FilePath path = fs::u8path(request.path());
    message.set_status(ExecuteFileListRequest(path, message.mutable_file_list()));

    status_dialog_->SetFileListRequestStatus(path, message.status());
    WriteMessage(message);
}

void FileTransferSessionClient::ReadCreateDirectoryRequest(
    const proto::CreateDirectoryRequest& request)
{
    proto::file_transfer::HostToClient message;

    FilePath path = fs::u8path(request.path());
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

    message.set_status(ExecuteRenameRequest(old_name, new_name));

    status_dialog_->SetRenameRequestStatus(old_name, new_name, message.status());
    WriteMessage(message);
}

void FileTransferSessionClient::ReadRemoveRequest(
    const proto::RemoveRequest& request)
{
    proto::file_transfer::HostToClient message;

    FilePath path = fs::u8path(request.path());
    message.set_status(ExecuteRemoveRequest(path));

    status_dialog_->SetRemoveRequestStatus(path, message.status());
    WriteMessage(message);
}

} // namespace aspia
