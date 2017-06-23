//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_session_file_transfer.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_session_file_transfer.h"
#include "base/strings/unicode.h"
#include "protocol/message_serialization.h"
#include "protocol/filesystem.h"

namespace aspia {

ClientSessionFileTransfer::ClientSessionFileTransfer(const ClientConfig& config,
                                                     ClientSession::Delegate* delegate) :
    ClientSession(config, delegate)
{
    worker_thread_.Start(MessageLoop::Type::TYPE_DEFAULT, this);
}

ClientSessionFileTransfer::~ClientSessionFileTransfer()
{
    worker_thread_.Stop();
}

void ClientSessionFileTransfer::OnBeforeThreadRunning()
{
    worker_ = worker_thread_.message_loop_proxy();
    DCHECK(worker_);

    file_manager_.reset(new UiFileManager(this));
}

void ClientSessionFileTransfer::OnAfterThreadRunning()
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
            file_manager_->ReadStatusCode(message.status());
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

void ClientSessionFileTransfer::OnDriveListRequest(UiFileManager::PanelType panel_type)
{
    if (!worker_->BelongsToCurrentThread())
    {
        worker_->PostTask(std::bind(&ClientSessionFileTransfer::OnDriveListRequest,
                                    this,
                                    panel_type));
        return;
    }

    proto::file_transfer::ClientToHost message;
    message.mutable_drive_list_request()->set_dummy(1);

    if (panel_type == UiFileManager::PanelType::REMOTE)
    {
        WriteMessage(message);
    }
    else
    {
        DCHECK(panel_type == UiFileManager::PanelType::LOCAL);

        std::unique_ptr<proto::DriveList> drive_list =
            ExecuteDriveListRequest(message.drive_list_request());

        if (drive_list)
        {
            file_manager_->ReadDriveList(UiFileManager::PanelType::LOCAL,
                                         std::move(drive_list));
        }
    }
}

void ClientSessionFileTransfer::OnDirectoryListRequest(UiFileManager::PanelType panel_type,
                                                       const std::string& path)
{
    if (!worker_->BelongsToCurrentThread())
    {
        worker_->PostTask(std::bind(&ClientSessionFileTransfer::OnDirectoryListRequest,
                                    this,
                                    panel_type,
                                    path));
        return;
    }

    proto::file_transfer::ClientToHost message;
    message.mutable_directory_list_request()->set_path(path);

    if (panel_type == UiFileManager::PanelType::REMOTE)
    {
        WriteMessage(message);
    }
    else
    {
        DCHECK(panel_type == UiFileManager::PanelType::LOCAL);

        std::unique_ptr<proto::DirectoryList> directory_list =
            ExecuteDirectoryListRequest(message.directory_list_request());

        if (directory_list)
        {
            file_manager_->ReadDirectoryList(UiFileManager::PanelType::LOCAL,
                                             std::move(directory_list));
        }
    }
}

void ClientSessionFileTransfer::OnCreateDirectoryRequest(
    UiFileManager::PanelType panel_type,
    const std::string& path)
{
    if (!worker_->BelongsToCurrentThread())
    {
        worker_->PostTask(std::bind(&ClientSessionFileTransfer::OnCreateDirectoryRequest,
                                    this,
                                    panel_type,
                                    path));
        return;
    }

    proto::file_transfer::ClientToHost message;
    message.mutable_create_directory_request()->set_path(path);

    if (panel_type == UiFileManager::PanelType::REMOTE)
    {
        WriteMessage(message);
    }
    else
    {
        DCHECK(panel_type == UiFileManager::PanelType::LOCAL);

        proto::Status status =
            ExecuteCreateDirectoryRequest(message.create_directory_request());
        if (status != proto::Status::STATUS_SUCCESS)
            file_manager_->ReadStatusCode(status);
    }
}

void ClientSessionFileTransfer::OnRenameRequest(
    UiFileManager::PanelType panel_type,
    const std::string& old_path,
    const std::string& new_path)
{
    if (!worker_->BelongsToCurrentThread())
    {
        worker_->PostTask(std::bind(&ClientSessionFileTransfer::OnRenameRequest,
                                    this,
                                    panel_type,
                                    old_path,
                                    new_path));
        return;
    }

    proto::file_transfer::ClientToHost message;

    message.mutable_rename_request()->set_old_path(old_path);
    message.mutable_rename_request()->set_new_path(new_path);

    if (panel_type == UiFileManager::PanelType::REMOTE)
    {
        WriteMessage(message);
    }
    else
    {
        DCHECK(panel_type == UiFileManager::PanelType::LOCAL);

        proto::Status status = ExecuteRenameRequest(message.rename_request());
        if (status != proto::Status::STATUS_SUCCESS)
            file_manager_->ReadStatusCode(status);
    }
}

void ClientSessionFileTransfer::OnRemoveRequest(
    UiFileManager::PanelType panel_type,
    const std::string& path)
{
    if (!worker_->BelongsToCurrentThread())
    {
        worker_->PostTask(std::bind(&ClientSessionFileTransfer::OnRemoveRequest,
                                    this,
                                    panel_type,
                                    path));
        return;
    }

    proto::file_transfer::ClientToHost message;
    message.mutable_remove_request()->set_path(path);

    if (panel_type == UiFileManager::PanelType::REMOTE)
    {
        WriteMessage(message);
    }
    else
    {
        DCHECK(panel_type == UiFileManager::PanelType::LOCAL);

        proto::Status status = ExecuteRemoveRequest(message.remove_request());
        if (status != proto::Status::STATUS_SUCCESS)
            file_manager_->ReadStatusCode(status);
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

    file_manager_->ReadDriveList(UiFileManager::PanelType::REMOTE,
                                 std::move(drive_list));
    return true;
}

bool ClientSessionFileTransfer::ReadDirectoryListMessage(
    std::unique_ptr<proto::DirectoryList> directory_list)
{
    if (!directory_list)
        return false;

    file_manager_->ReadDirectoryList(UiFileManager::PanelType::REMOTE,
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
        delegate_->OnSessionMessage(std::move(buffer));
        return;
    }

    delegate_->OnSessionTerminate();
}

} // namespace aspia
