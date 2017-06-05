//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_session_file_transfer.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_session_file_transfer.h"
#include "protocol/message_serialization.h"

namespace aspia {

ClientSessionFileTransfer::ClientSessionFileTransfer(const ClientConfig& config,
                                                     ClientSession::Delegate* delegate) :
    ClientSession(config, delegate)
{
    file_manager_.reset(new FileManager(this));
}

ClientSessionFileTransfer::~ClientSessionFileTransfer()
{
    file_manager_.reset();
}

void ClientSessionFileTransfer::Send(const IOBuffer& buffer)
{
    proto::file_transfer::HostToClient message;

    if (ParseMessage(buffer, message))
    {
        bool success = true;

        if (message.has_drive_list())
        {
            success = ReadDriveListMessage(message.drive_list());
        }
        else if (message.has_directory_list())
        {
            success = ReadDirectoryListMessage(message.directory_list());
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

    delegate_->OnSessionTerminate();
}

void ClientSessionFileTransfer::OnWindowClose()
{
    delegate_->OnSessionTerminate();
}

bool ClientSessionFileTransfer::ReadDriveListMessage(
    const proto::DriveList& drive_list)
{
    return true;
}

bool ClientSessionFileTransfer::ReadDirectoryListMessage(
    const proto::DirectoryList& direcrory_list)
{
    return true;
}

bool ClientSessionFileTransfer::ReadFileMessage(const proto::File& file)
{
    return true;
}

} // namespace aspia
