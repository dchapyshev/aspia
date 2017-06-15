//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/file_transfer_session_client.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/file_transfer_session_client.h"
#include "base/drive_enumerator.h"
#include "base/file_enumerator.h"
#include "base/unicode.h"
#include "base/path.h"
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
    proto::file_transfer::HostToClient message;

    DriveEnumerator enumerator;

    for (;;)
    {
        std::wstring path = enumerator.Next();

        if (path.empty())
            break;

        proto::DriveListItem* item = message.mutable_drive_list()->add_item();

        item->set_path(UTF8fromUNICODE(path));

        DriveEnumerator::DriveInfo drive_info = enumerator.GetInfo();

        item->set_name(UTF8fromUNICODE(drive_info.VolumeName()));

        switch (drive_info.Type())
        {
            case DriveEnumerator::DriveInfo::DriveType::CDROM:
                item->set_type(proto::DriveListItem::CDROM);
                break;

            case DriveEnumerator::DriveInfo::DriveType::REMOVABLE:
                item->set_type(proto::DriveListItem::REMOVABLE);
                break;

            case DriveEnumerator::DriveInfo::DriveType::FIXED:
                item->set_type(proto::DriveListItem::FIXED);
                break;

            case DriveEnumerator::DriveInfo::DriveType::RAM:
                item->set_type(proto::DriveListItem::RAM);
                break;

            case DriveEnumerator::DriveInfo::DriveType::REMOTE:
                item->set_type(proto::DriveListItem::REMOTE);
                break;

            default:
                item->set_type(proto::DriveListItem::UNKNOWN);
                break;
        }
    }

    std::wstring path;

    if (GetPathW(PathKey::DIR_USER_HOME, path))
    {
        proto::DriveListItem* item = message.mutable_drive_list()->add_item();

        item->set_type(proto::DriveListItem::HOME_FOLDER);
        item->set_path(UTF8fromUNICODE(path));
    }

    if (GetPathW(PathKey::DIR_USER_DESKTOP, path))
    {
        proto::DriveListItem* item = message.mutable_drive_list()->add_item();

        item->set_type(proto::DriveListItem::DESKTOP_FOLDER);
        item->set_path(UTF8fromUNICODE(path));
    }

    WriteMessage(message);
    return true;
}

bool FileTransferSessionClient::ReadDirectoryListRequestMessage(
    const proto::DirectoryListRequest& direcrory_list_request)
{
    proto::file_transfer::HostToClient message;

    FileEnumerator enumerator(UNICODEfromUTF8(direcrory_list_request.path()),
                              false,
                              FileEnumerator::FILES | FileEnumerator::DIRECTORIES);

    for (;;)
    {
        std::wstring path = enumerator.Next();

        if (path.empty())
            break;

        proto::DirectoryListItem* item = message.mutable_directory_list()->add_item();
        FileEnumerator::FileInfo file_info = enumerator.GetInfo();

        item->set_name(UTF8fromUNICODE(file_info.GetName()));

        if (!file_info.IsDirectory())
        {
            item->set_type(proto::DirectoryListItem::FILE);
            item->set_size(file_info.GetSize());
        }
        else
        {
            item->set_type(proto::DirectoryListItem::DIRECTORY);
        }
    }

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

} // namespace aspia
