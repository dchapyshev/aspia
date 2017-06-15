//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_session_file_transfer.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_session_file_transfer.h"
#include "base/drive_enumerator.h"
#include "base/file_enumerator.h"
#include "base/unicode.h"
#include "base/path.h"
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
        if (message.status() != proto::Status::STATUS_SUCCESS)
        {
            // TODO: Status processing.
            return;
        }

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

        DriveEnumerator enumerator;

        for (;;)
        {
            std::wstring path = enumerator.Next();

            if (path.empty())
                break;

            DriveEnumerator::DriveInfo drive_info = enumerator.GetInfo();

            proto::DriveListItem::Type drive_type;

            switch (drive_info.Type())
            {
                case DriveEnumerator::DriveInfo::DriveType::CDROM:
                    drive_type = proto::DriveListItem::CDROM;
                    break;

                case DriveEnumerator::DriveInfo::DriveType::REMOVABLE:
                    drive_type = proto::DriveListItem::REMOVABLE;
                    break;

                case DriveEnumerator::DriveInfo::DriveType::FIXED:
                    drive_type = proto::DriveListItem::FIXED;
                    break;

                case DriveEnumerator::DriveInfo::DriveType::RAM:
                    drive_type = proto::DriveListItem::RAM;
                    break;

                case DriveEnumerator::DriveInfo::DriveType::REMOTE:
                    drive_type = proto::DriveListItem::REMOTE;
                    break;

                default:
                    drive_type = proto::DriveListItem::UNKNOWN;
                    break;
            }

            file_manager_->AddDriveItem(panel_type,
                                        drive_type,
                                        drive_info.Path(),
                                        drive_info.VolumeName());
        }

        std::wstring path;

        if (GetPathW(PathKey::DIR_USER_HOME, path))
        {
            file_manager_->AddDriveItem(panel_type,
                                        proto::DriveListItem::HOME_FOLDER,
                                        path,
                                        std::wstring());
        }

        if (GetPathW(PathKey::DIR_USER_DESKTOP, path))
        {
            file_manager_->AddDriveItem(panel_type,
                                        proto::DriveListItem::DESKTOP_FOLDER,
                                        path,
                                        std::wstring());
        }
    }
}

void ClientSessionFileTransfer::OnDirectoryListRequest(FileManager::PanelType panel_type,
                                                       const std::wstring& path)
{
    if (panel_type == FileManager::PanelType::REMOTE)
    {
        proto::file_transfer::ClientToHost message;
        message.mutable_directory_list_request()->set_path(UTF8fromUNICODE(path));
        WriteMessage(message);
    }
    else
    {
        DCHECK(panel_type == FileManager::PanelType::LOCAL);

        FileEnumerator enumerator(path,
                                  false,
                                  FileEnumerator::FILES | FileEnumerator::DIRECTORIES);

        for (;;)
        {
            std::wstring path = enumerator.Next();

            if (path.empty())
                break;

            FileEnumerator::FileInfo file_info = enumerator.GetInfo();

            proto::DirectoryListItem::Type type;

            if (file_info.IsDirectory())
            {
                type = proto::DirectoryListItem::DIRECTORY;
            }
            else
            {
                type = proto::DirectoryListItem::FILE;
            }

            file_manager_->AddDirectoryItem(panel_type,
                                            type,
                                            file_info.GetName(),
                                            file_info.GetSize());
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
    const proto::DriveList& drive_list)
{
    const int count = drive_list.item_size();

    for (int index = 0; index < count; ++index)
    {
        const proto::DriveListItem& item = drive_list.item(index);

        file_manager_->AddDriveItem(FileManager::PanelType::REMOTE,
                                    item.type(),
                                    UNICODEfromUTF8(item.path()),
                                    UNICODEfromUTF8(item.name()));
    }

    return true;
}

bool ClientSessionFileTransfer::ReadDirectoryListMessage(
    const proto::DirectoryList& direcrory_list)
{
    const int count = direcrory_list.item_size();

    for (int index = 0; index < count; ++index)
    {
        const proto::DirectoryListItem& item = direcrory_list.item(index);

        file_manager_->AddDirectoryItem(FileManager::PanelType::REMOTE,
                                        item.type(),
                                        UNICODEfromUTF8(item.name()),
                                        item.size());
    }

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
