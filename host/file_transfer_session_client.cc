//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/file_transfer_session_client.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/file_transfer_session_client.h"
#include "base/unicode.h"
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
            success = ReadDirectoryListRequestMessage(message.directory_list_request());
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
            success = ReadCreateDirectoryRequest(message.create_directory_request());
        }
        else if (message.has_rename_request())
        {
            success = ReadRenameRequest(message.rename_request());
        }
        else if (message.has_remove_request())
        {
            success = ReadRemoveRequest(message.remove_request());
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
    std::unique_ptr<proto::DriveList> drive_list = CreateDriveList();
    if (!drive_list)
        return false;

    proto::file_transfer::HostToClient message;
    message.set_allocated_drive_list(drive_list.release());
    WriteMessage(message);

    return true;
}

bool FileTransferSessionClient::ReadDirectoryListRequestMessage(
    const proto::DirectoryListRequest& direcrory_list_request)
{
    std::experimental::filesystem::path path =
        std::experimental::filesystem::u8path(direcrory_list_request.path());

    status_dialog_->OnDirectoryOpen(path.wstring());

    std::unique_ptr<proto::DirectoryList> directory_list =
        CreateDirectoryList(path);

    if (!directory_list)
        return false;

    proto::file_transfer::HostToClient message;
    message.set_allocated_directory_list(directory_list.release());
    WriteMessage(message);

    return true;
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

bool FileTransferSessionClient::ReadCreateDirectoryRequest(
    const proto::CreateDirectoryRequest& create_directory_request)
{
    std::experimental::filesystem::path path =
        std::experimental::filesystem::u8path(create_directory_request.path());

    std::error_code code;
    std::experimental::filesystem::create_directory(path, code);

    return true;
}

bool FileTransferSessionClient::ReadRenameRequest(
    const proto::RenameRequest& rename_request)
{
    std::experimental::filesystem::path old_path =
        std::experimental::filesystem::u8path(rename_request.old_path());

    std::experimental::filesystem::path new_path =
        std::experimental::filesystem::u8path(rename_request.new_path());

    status_dialog_->OnRename(old_path.wstring(), new_path.wstring());

    std::error_code code;
    std::experimental::filesystem::rename(old_path, new_path, code);

    return true;
}

bool FileTransferSessionClient::ReadRemoveRequest(
    const proto::RemoveRequest& remove_request)
{
    std::experimental::filesystem::path path =
        std::experimental::filesystem::u8path(remove_request.path());

    status_dialog_->OnRemove(path.wstring());

    std::error_code code;
    std::experimental::filesystem::remove(path, code);

    return true;
}

} // namespace aspia
