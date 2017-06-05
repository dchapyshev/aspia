//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_session_file_transfer.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_session_file_transfer.h"
#include "protocol/message_serialization.h"

namespace aspia {

HostSessionFileTransfer::HostSessionFileTransfer(HostSession::Delegate* delegate) :
    HostSession(delegate)
{
    thread_.Start(MessageLoop::Type::TYPE_DEFAULT, this);
    runner_ = thread_.message_loop_proxy();
    DCHECK(runner_);
}

HostSessionFileTransfer::~HostSessionFileTransfer()
{
    thread_.Stop();
}

void HostSessionFileTransfer::OnBeforeThreadRunning()
{

}

void HostSessionFileTransfer::OnAfterThreadRunning()
{
    delegate_->OnSessionTerminate();
}

void HostSessionFileTransfer::Send(const IOBuffer& buffer)
{
    proto::file_transfer::ClientToHost message;

    if (ParseMessage(buffer, message))
    {
        if (message.has_drive_list_request())
        {
            ReadDriveListRequestMessage(message.drive_list_request());
        }
        else if (message.has_directory_list_request())
        {
            ReadDirectoryListRequestMessage(message.directory_list_request());
        }
        else if (message.has_file_request())
        {
            ReadFileRequestMessage(message.file_request());
        }
        else if (message.has_file())
        {
            ReadFileMessage(message.file());
        }
        else
        {
            // Unknown messages are ignored.
            DLOG(WARNING) << "Unhandled message from host";
        }
    }
}

void HostSessionFileTransfer::ReadDriveListRequestMessage(
    const proto::DriveListRequest& drive_list_request)
{

}

void HostSessionFileTransfer::ReadDirectoryListRequestMessage(
    const proto::DirectoryListRequest& direcrory_request)
{

}

void HostSessionFileTransfer::ReadFileRequestMessage(
    const proto::FileRequest& file_request)
{

}

void HostSessionFileTransfer::ReadFileMessage(const proto::File& file)
{

}

} // namespace aspia
