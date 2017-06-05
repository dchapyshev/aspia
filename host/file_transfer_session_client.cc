//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/file_transfer_session_client.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/file_transfer_session_client.h"
#include "protocol/message_serialization.h"
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

void FileTransferSessionClient::OnPipeChannelConnect(uint32_t user_data)
{
    // The server sends the session type in user_data.
    proto::SessionType session_type = static_cast<proto::SessionType>(user_data);

    if (session_type != proto::SessionType::SESSION_TYPE_FILE_TRANSFER)
    {
        LOG(FATAL) << "Invalid session type passed: " << session_type;
        return;
    }
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
    IOBuffer buffer(SerializeMessage(message));
    std::lock_guard<std::mutex> lock(outgoing_lock_);
    ipc_channel_->Send(buffer);
}

bool FileTransferSessionClient::ReadDriveListRequestMessage(
    const proto::DriveListRequest& drive_list_request)
{
    return true;
}

bool FileTransferSessionClient::ReadDirectoryListRequestMessage(
    const proto::DirectoryListRequest& direcrory_list_request)
{
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

} // namespace aspia
