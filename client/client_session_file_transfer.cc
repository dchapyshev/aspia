//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_session_file_transfer.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_session_file_transfer.h"
#include "base/unicode.h"
#include "protocol/message_serialization.h"
#include "protocol/directory_list.h"
#include "protocol/drive_list.h"

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
        if (message.status() != proto::Status::STATUS_SUCCESS)
        {
            // TODO: Status processing.
            return;
        }

        bool success = true;

        if (message.has_drive_list())
        {
            std::unique_ptr<proto::DriveList> drive_list(message.release_drive_list());
            success = ReadDriveListMessage(std::move(drive_list));
        }
        else if (message.has_directory_list())
        {
            std::unique_ptr<proto::DirectoryList> directory_list(message.release_directory_list());
            success = ReadDirectoryListMessage(std::move(directory_list));
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

void ClientSessionFileTransfer::OnDriveListRequest(FileManager::PanelType panel_type)
{
    if (panel_type == FileManager::PanelType::REMOTE)
    {
        proto::file_transfer::ClientToHost message;
        message.mutable_drive_list_request()->set_dummy(1);
        WriteMessage(message);
    }
    else
    {
        DCHECK(panel_type == FileManager::PanelType::LOCAL);

        std::unique_ptr<proto::DriveList> drive_list = CreateDriveList();

        if (drive_list)
        {
            file_manager_->ReadDriveList(FileManager::PanelType::LOCAL,
                                         std::move(drive_list));
        }
    }
}

void ClientSessionFileTransfer::OnDirectoryListRequest(FileManager::PanelType panel_type,
                                                       const std::string& path)
{
    if (panel_type == FileManager::PanelType::REMOTE)
    {
        proto::file_transfer::ClientToHost message;
        message.mutable_directory_list_request()->set_path(path);
        WriteMessage(message);
    }
    else
    {
        DCHECK(panel_type == FileManager::PanelType::LOCAL);

        std::unique_ptr<proto::DirectoryList> directory_list =
            CreateDirectoryList(UNICODEfromUTF8(path));

        if (directory_list)
        {
            file_manager_->ReadDirectoryList(FileManager::PanelType::LOCAL,
                                             std::move(directory_list));
        }
    }
}

void ClientSessionFileTransfer::OnSendFile(const std::wstring& from_path,
                                           const std::wstring& to_path)
{

}

void ClientSessionFileTransfer::OnRecieveFile(const std::wstring& from_path,
                                              const std::wstring& to_path)
{

}

bool ClientSessionFileTransfer::ReadDriveListMessage(
    std::unique_ptr<proto::DriveList> drive_list)
{
    if (!drive_list)
        return false;

    file_manager_->ReadDriveList(FileManager::PanelType::REMOTE,
                                 std::move(drive_list));
    return true;
}

bool ClientSessionFileTransfer::ReadDirectoryListMessage(
    std::unique_ptr<proto::DirectoryList> directory_list)
{
    if (!directory_list)
        return false;

    file_manager_->ReadDirectoryList(FileManager::PanelType::REMOTE,
                                     std::move(directory_list));
    return true;
}

bool ClientSessionFileTransfer::ReadFileMessage(const proto::File& file)
{
    return true;
}

void ClientSessionFileTransfer::WriteMessage(
    const proto::file_transfer::ClientToHost& message)
{
    IOBuffer buffer(SerializeMessage<IOBuffer>(message));

    if (!buffer.IsEmpty())
    {
        delegate_->OnSessionMessageAsync(std::move(buffer));
        return;
    }

    delegate_->OnSessionTerminate();
}

} // namespace aspia
