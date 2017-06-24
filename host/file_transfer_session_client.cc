//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/file_transfer_session_client.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/file_transfer_session_client.h"
#include "base/strings/unicode.h"
#include "protocol/message_serialization.h"
#include "protocol/filesystem.h"
#include "proto/auth_session.pb.h"

namespace aspia {

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
        else if (message.has_directory_list_request())
        {
            ReadDirectoryListRequestMessage(message.directory_list_request());
        }
        else if (message.has_file_request())
        {
            success = ReadFileRequestMessage(message.file_request());
        }
        else if (message.has_file())
        {
            success = ReadFileMessage(message.file());
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
    DCHECK(status != proto::Status::STATUS_SUCCESS);

    proto::file_transfer::HostToClient message;
    message.set_status(status);
    WriteMessage(message);
}

bool FileTransferSessionClient::ReadDriveListRequestMessage(
    const proto::DriveListRequest& drive_list_request)
{
    std::unique_ptr<proto::DriveList> drive_list =
        ExecuteDriveListRequest(drive_list_request);
    if (!drive_list)
        return false;

    proto::file_transfer::HostToClient message;
    message.set_allocated_drive_list(drive_list.release());
    WriteMessage(message);

    return true;
}

void FileTransferSessionClient::ReadDirectoryListRequestMessage(
    const proto::DirectoryListRequest& direcrory_list_request)
{
    status_dialog_->OnDirectoryOpen(
        UNICODEfromUTF8(direcrory_list_request.path()));

    std::unique_ptr<proto::DirectoryList> directory_list =
        ExecuteDirectoryListRequest(direcrory_list_request);

    if (!directory_list)
        return;

    proto::file_transfer::HostToClient message;
    message.set_allocated_directory_list(directory_list.release());
    WriteMessage(message);
}

bool FileTransferSessionClient::ReadFileRequestMessage(
    const proto::FileRequest& file_request)
{
    return true;
}

bool FileTransferSessionClient::ReadFileMessage(const proto::File& file)
{
    return true;
}

void FileTransferSessionClient::ReadCreateDirectoryRequest(
    const proto::CreateDirectoryRequest& create_directory_request)
{
    proto::Status status =
        ExecuteCreateDirectoryRequest(create_directory_request);

    status_dialog_->OnCreateDirectory(UNICODEfromUTF8(create_directory_request.path()),
                                      UNICODEfromUTF8(create_directory_request.name()),
                                      status);

    if (status != proto::Status::STATUS_SUCCESS)
        WriteStatus(status);
}

void FileTransferSessionClient::ReadRenameRequest(
    const proto::RenameRequest& rename_request)
{
    proto::Status status = ExecuteRenameRequest(rename_request);

    status_dialog_->OnRename(UNICODEfromUTF8(rename_request.path()),
                             UNICODEfromUTF8(rename_request.old_item_name()),
                             UNICODEfromUTF8(rename_request.new_item_name()),
                             status);

    if (status != proto::Status::STATUS_SUCCESS)
        WriteStatus(status);
}

void FileTransferSessionClient::ReadRemoveRequest(
    const proto::RemoveRequest& remove_request)
{
    proto::Status status = ExecuteRemoveRequest(remove_request);

    status_dialog_->OnRemove(UNICODEfromUTF8(remove_request.path()),
                             UNICODEfromUTF8(remove_request.item_name()),
                             status);

    if (status != proto::Status::STATUS_SUCCESS)
        WriteStatus(status);
}

} // namespace aspia
