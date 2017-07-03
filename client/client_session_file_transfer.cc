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

#include <memory>

namespace aspia {

ClientSessionFileTransfer::ClientSessionFileTransfer(const ClientConfig& config,
                                                     ClientSession::Delegate* delegate) :
    ClientSession(config, delegate),
    message_reply_event_(WaitableEvent::ResetPolicy::AUTOMATIC,
                         WaitableEvent::InitialState::NOT_SIGNALED)
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
    std::unique_ptr<proto::file_transfer::HostToClient> message =
        std::make_unique<proto::file_transfer::HostToClient>();

    if (ParseMessage(buffer, *message))
    {
        std::lock_guard<std::mutex> lock(message_reply_lock_);
        message_reply_.swap(message);
    }

    message_reply_event_.Signal();
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

    proto::file_transfer::ClientToHost request;
    request.mutable_drive_list_request()->set_dummy(1);

    std::unique_ptr<proto::file_transfer::HostToClient> reply;

    if (panel_type == UiFileManager::PanelType::REMOTE)
    {
        reply = SendRemoteRequest(request);
    }
    else
    {
        DCHECK(panel_type == UiFileManager::PanelType::LOCAL);
        reply = SendLocalRequest(request);
    }

    if (reply)
    {
        if (reply->has_status())
        {
            std::unique_ptr<proto::RequestStatus> status(reply->release_status());
            file_manager_->ReadRequestStatus(std::move(status));
        }
        else if (reply->has_drive_list())
        {
            std::unique_ptr<proto::DriveList> drive_list(reply->release_drive_list());
            file_manager_->ReadDriveList(panel_type, std::move(drive_list));
        }
    }
}

void ClientSessionFileTransfer::OnDirectoryListRequest(UiFileManager::PanelType panel_type,
                                                       const std::string& path,
                                                       const std::string& item)
{
    if (!worker_->BelongsToCurrentThread())
    {
        worker_->PostTask(std::bind(&ClientSessionFileTransfer::OnDirectoryListRequest,
                                    this,
                                    panel_type,
                                    path,
                                    item));
        return;
    }

    proto::file_transfer::ClientToHost request;
    request.mutable_directory_list_request()->set_path(path);
    request.mutable_directory_list_request()->set_item(item);

    std::unique_ptr<proto::file_transfer::HostToClient> reply;

    if (panel_type == UiFileManager::PanelType::REMOTE)
    {
        reply = SendRemoteRequest(request);
    }
    else
    {
        DCHECK(panel_type == UiFileManager::PanelType::LOCAL);
        reply = SendLocalRequest(request);
    }

    if (reply)
    {
        if (reply->has_status())
        {
            std::unique_ptr<proto::RequestStatus> status(reply->release_status());
            file_manager_->ReadRequestStatus(std::move(status));
        }
        else if (reply->has_directory_list())
        {
            std::unique_ptr<proto::DirectoryList> directory_list(reply->release_directory_list());
            file_manager_->ReadDirectoryList(panel_type, std::move(directory_list));
        }
    }
}

void ClientSessionFileTransfer::OnCreateDirectoryRequest(
    UiFileManager::PanelType panel_type,
    const std::string& path,
    const std::string& name)
{
    if (!worker_->BelongsToCurrentThread())
    {
        worker_->PostTask(std::bind(&ClientSessionFileTransfer::OnCreateDirectoryRequest,
                                    this,
                                    panel_type,
                                    path,
                                    name));
        return;
    }

    proto::file_transfer::ClientToHost request;
    request.mutable_create_directory_request()->set_path(path);
    request.mutable_create_directory_request()->set_name(name);

    std::unique_ptr<proto::file_transfer::HostToClient> reply;

    if (panel_type == UiFileManager::PanelType::REMOTE)
    {
        reply = SendRemoteRequest(request);
    }
    else
    {
        DCHECK(panel_type == UiFileManager::PanelType::LOCAL);
        reply = SendLocalRequest(request);
    }

    if (reply)
    {
        if (reply->status().code() != proto::Status::STATUS_SUCCESS)
        {
            std::unique_ptr<proto::RequestStatus> status(reply->release_status());
            file_manager_->ReadRequestStatus(std::move(status));
        }
    }
}

void ClientSessionFileTransfer::OnRenameRequest(
    UiFileManager::PanelType panel_type,
    const std::string& path,
    const std::string& old_name,
    const std::string& new_name)
{
    if (!worker_->BelongsToCurrentThread())
    {
        worker_->PostTask(std::bind(&ClientSessionFileTransfer::OnRenameRequest,
                                    this,
                                    panel_type,
                                    path,
                                    old_name,
                                    new_name));
        return;
    }

    proto::file_transfer::ClientToHost request;

    request.mutable_rename_request()->set_path(path);
    request.mutable_rename_request()->set_old_item_name(old_name);
    request.mutable_rename_request()->set_new_item_name(new_name);

    std::unique_ptr<proto::file_transfer::HostToClient> reply;

    if (panel_type == UiFileManager::PanelType::REMOTE)
    {
        reply = SendRemoteRequest(request);
    }
    else
    {
        DCHECK(panel_type == UiFileManager::PanelType::LOCAL);
        reply = SendLocalRequest(request);
    }

    if (reply)
    {
        if (reply->status().code() != proto::Status::STATUS_SUCCESS)
        {
            std::unique_ptr<proto::RequestStatus> status(reply->release_status());
            file_manager_->ReadRequestStatus(std::move(status));
        }
    }
}

void ClientSessionFileTransfer::OnRemoveRequest(
    UiFileManager::PanelType panel_type,
    const std::string& path,
    const std::string& item_name)
{
    if (!worker_->BelongsToCurrentThread())
    {
        worker_->PostTask(std::bind(&ClientSessionFileTransfer::OnRemoveRequest,
                                    this,
                                    panel_type,
                                    path,
                                    item_name));
        return;
    }

    proto::file_transfer::ClientToHost request;
    request.mutable_remove_request()->set_path(path);
    request.mutable_remove_request()->set_item_name(item_name);

    std::unique_ptr<proto::file_transfer::HostToClient> reply;

    if (panel_type == UiFileManager::PanelType::REMOTE)
    {
        reply = SendRemoteRequest(request);
    }
    else
    {
        DCHECK(panel_type == UiFileManager::PanelType::LOCAL);
        reply = SendLocalRequest(request);
    }

    if (reply)
    {
        if (reply->status().code() != proto::Status::STATUS_SUCCESS)
        {
            std::unique_ptr<proto::RequestStatus> status(reply->release_status());
            file_manager_->ReadRequestStatus(std::move(status));
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

bool ClientSessionFileTransfer::ReadFilePacketMessage(const proto::FilePacket& file)
{
    return true;
}

std::unique_ptr<proto::file_transfer::HostToClient>
ClientSessionFileTransfer::SendRemoteRequest(
    const proto::file_transfer::ClientToHost& request)
{
    DCHECK(worker_->BelongsToCurrentThread());

    std::unique_ptr<proto::file_transfer::HostToClient> reply;

    IOBuffer buffer(SerializeMessage<IOBuffer>(request));

    if (!buffer.IsEmpty())
    {
        delegate_->OnSessionMessage(std::move(buffer));
        message_reply_event_.Wait();

        std::lock_guard<std::mutex> lock(message_reply_lock_);
        reply.swap(message_reply_);
    }

    if (!reply)
    {
        delegate_->OnSessionTerminate();
    }

    return reply;
}

std::unique_ptr<proto::file_transfer::HostToClient>
ClientSessionFileTransfer::SendLocalRequest(
    const proto::file_transfer::ClientToHost& request)
{
    DCHECK(worker_->BelongsToCurrentThread());

    std::unique_ptr<proto::file_transfer::HostToClient> reply =
        std::make_unique<proto::file_transfer::HostToClient>();

    std::unique_ptr<proto::RequestStatus> status;

    if (request.has_drive_list_request())
    {
        std::unique_ptr<proto::DriveList> drive_list;

        status = ExecuteDriveListRequest(request.drive_list_request(),
                                         drive_list);
        DCHECK(status);

        if (status->code() == proto::Status::STATUS_SUCCESS)
        {
            reply->set_allocated_drive_list(drive_list.release());
        }
        else
        {
            reply->set_allocated_status(status.release());
        }
    }
    else if (request.has_directory_list_request())
    {
        std::unique_ptr<proto::DirectoryList> directory_list;

        status = ExecuteDirectoryListRequest(request.directory_list_request(),
                                             directory_list);
        DCHECK(status);

        if (status->code() == proto::Status::STATUS_SUCCESS)
        {
            reply->set_allocated_directory_list(directory_list.release());
        }
        else
        {
            reply->set_allocated_status(status.release());
        }
    }
    else if (request.has_create_directory_request())
    {
        status = ExecuteCreateDirectoryRequest(request.create_directory_request());
        DCHECK(status);

        reply->set_allocated_status(status.release());
    }
    else if (request.has_rename_request())
    {
        status = ExecuteRenameRequest(request.rename_request());
        DCHECK(status);

        reply->set_allocated_status(status.release());
    }
    else if (request.has_remove_request())
    {
        status = ExecuteRemoveRequest(request.remove_request());
        DCHECK(status);

        reply->set_allocated_status(status.release());
    }
    else
    {
        // Unknown messages are ignored.
        DLOG(WARNING) << "Unhandled request";
        return nullptr;
    }

    return reply;
}

} // namespace aspia
