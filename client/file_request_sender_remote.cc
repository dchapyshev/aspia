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

FileRequestSenderRemote::FileRequestSenderRemote(ClientSession::Delegate* session) :
    session_(session)
{
    DCHECK(session_);
}

void FileRequestSenderRemote::SendDriveListRequest(std::shared_ptr<FileReplyReceiverProxy> receiver)
{
    proto::file_transfer::ClientToHost request;
    request.mutable_drive_list_request()->set_dummy(1);
    SendRequest(receiver, request);
}

void FileRequestSenderRemote::SendFileListRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                                  const FilePath& path)
{
    proto::file_transfer::ClientToHost request;
    request.mutable_file_list_request()->set_path(path.u8string());
    SendRequest(receiver, request);
}

void FileRequestSenderRemote::SendCreateDirectoryRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                                         const FilePath& path)
{
    proto::file_transfer::ClientToHost request;
    request.mutable_create_directory_request()->set_path(path.u8string());
    SendRequest(receiver, request);
}

void FileRequestSenderRemote::SendDirectorySizeRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                                       const FilePath& path)
{
    // TODO
}

void FileRequestSenderRemote::SendRemoveRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                                const FilePath& path)
{
    proto::file_transfer::ClientToHost request;
    request.mutable_remove_request()->set_path(path.u8string());
    SendRequest(receiver, request);
}

void FileRequestSenderRemote::SendRenameRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                                const FilePath& old_name,
                                                const FilePath& new_name)
{
    proto::file_transfer::ClientToHost request;

    request.mutable_rename_request()->set_old_name(old_name.u8string());
    request.mutable_rename_request()->set_new_name(new_name.u8string());

    SendRequest(receiver, request);
}

bool FileRequestSenderRemote::ReadIncommingMessage(const IOBuffer& buffer)
{
    std::shared_ptr<FileReplyReceiverProxy> receiver = receiver_queue_.NextReceiver();

    if (!receiver)
    {
        LOG(ERROR) << "Unexpected message received. Receiver queue is empty";
        session_->OnSessionTerminate();
        return false;
    }

    proto::file_transfer::HostToClient message;

    if (!ParseMessage(buffer, message))
    {
        session_->OnSessionTerminate();
        return false;
    }

    if (message.status() != proto::Status::STATUS_SUCCESS)
    {
        receiver->OnLastRequestFailed(message.status());
    }
    else if (message.has_drive_list())
    {
        std::unique_ptr<proto::DriveList> drive_list(message.release_drive_list());
        receiver->OnDriveListReply(std::move(drive_list));
    }
    else if (message.has_file_list())
    {
        std::unique_ptr<proto::FileList> file_list(message.release_file_list());
        receiver->OnFileListReply(std::move(file_list));
    }
    else
    {
        DLOG(WARNING) << "Unhandled message from host";
    }

    return true;
}

void FileRequestSenderRemote::SendRequest(std::shared_ptr<FileReplyReceiverProxy> receiver,
                                          const proto::file_transfer::ClientToHost& request)
{
    receiver_queue_.AddReceiver(receiver);
    session_->OnSessionMessageAsync(SerializeMessage<IOBuffer>(request));
}

} // namespace aspia
