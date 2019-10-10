//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "client/client_file_transfer.h"

#include "base/logging.h"
#include "client/file_control_proxy.h"
#include "client/file_manager_window_proxy.h"
#include "client/file_request_factory.h"
#include "common/file_request.h"
#include "common/file_request_consumer_proxy.h"
#include "common/file_request_producer_proxy.h"
#include "common/file_worker.h"

namespace client {

ClientFileTransfer::ClientFileTransfer(std::shared_ptr<base::TaskRunner>& ui_task_runner)
    : Client(ui_task_runner)
{
    // Nothing
}

ClientFileTransfer::~ClientFileTransfer() = default;

void ClientFileTransfer::setFileManagerWindow(FileManagerWindow* file_manager_window)
{
    DCHECK(!ioTaskRunner());
    DCHECK(uiTaskRunner()->belongsToCurrentThread());

    file_manager_window_proxy_ =
        FileManagerWindowProxy::create(uiTaskRunner(), file_manager_window);
}

void ClientFileTransfer::onSessionStarted(const base::Version& /* peer_version */)
{
    request_consumer_proxy_ = std::make_shared<common::FileRequestConsumerProxy>(this);
    request_producer_proxy_ = std::make_shared<common::FileRequestProducerProxy>(this);

    local_request_factory_ = std::make_unique<FileRequestFactory>(
        request_producer_proxy_, common::FileTaskTarget::LOCAL);

    remote_request_factory_ = std::make_unique<FileRequestFactory>(
        request_producer_proxy_, common::FileTaskTarget::REMOTE);

    file_control_proxy_ = std::make_shared<FileControlProxy>(ioTaskRunner(), this);
    local_worker_ = std::make_unique<common::FileWorker>(ioTaskRunner());

    file_manager_window_proxy_->start(file_control_proxy_);
}

void ClientFileTransfer::onSessionStopped()
{
    request_consumer_proxy_->dettach();
    request_producer_proxy_->dettach();
    file_control_proxy_->dettach();

    remover_.reset();
    transfer_.reset();
}

void ClientFileTransfer::onMessageReceived(const base::ByteArray& buffer)
{
    std::unique_ptr<proto::FileReply> reply = std::make_unique<proto::FileReply>();

    if (!reply->ParseFromArray(buffer.data(), buffer.size()))
    {
        LOG(LS_ERROR) << "Invalid message from host";
        return;
    }

    // Move the reply to the request and notify the sender.
    remote_request_queue_.front()->setReply(std::move(reply));

    // Remove the request from the queue.
    remote_request_queue_.pop();

    // Execute the next request.
    doNextRemoteRequest();
}

void ClientFileTransfer::onMessageWritten()
{
    // Nothing
}

void ClientFileTransfer::onReply(std::shared_ptr<common::FileRequest> request)
{
    const proto::FileRequest& file_request = request->request();
    const proto::FileReply& file_reply = request->reply();

    if (file_request.has_drive_list_request())
    {
        file_manager_window_proxy_->onDriveList(
            request->target(), file_reply.error_code(), file_reply.drive_list());
    }
    else if (file_request.has_file_list_request())
    {
        file_manager_window_proxy_->onFileList(
            request->target(), file_reply.error_code(), file_reply.file_list());
    }
    else if (file_request.has_create_directory_request())
    {
        file_manager_window_proxy_->onCreateDirectory(request->target(), file_reply.error_code());
    }
    else if (file_request.has_rename_request())
    {
        file_manager_window_proxy_->onRename(request->target(), file_reply.error_code());
    }
    else
    {
        NOTREACHED();
    }
}

void ClientFileTransfer::doRequest(std::shared_ptr<common::FileRequest> request)
{
    if (request->target() == common::FileTaskTarget::LOCAL)
    {
        local_worker_->sendRequest(request);
    }
    else
    {
        const bool schedule = remote_request_queue_.empty();

        // Add the request to the queue.
        remote_request_queue_.emplace(request);

        // If the request queue was empty, then run execution.
        if (schedule)
            doNextRemoteRequest();
    }
}

void ClientFileTransfer::doNextRemoteRequest()
{
    if (remote_request_queue_.empty())
        return;

    // Send a request to the remote computer.
    sendMessage(remote_request_queue_.front()->request());
}

FileRequestFactory* ClientFileTransfer::requestFactory(common::FileTaskTarget target)
{
    FileRequestFactory* request_factory;

    if (target == common::FileTaskTarget::LOCAL)
    {
        request_factory = local_request_factory_.get();
    }
    else
    {
        DCHECK_EQ(target, common::FileTaskTarget::REMOTE);
        request_factory = remote_request_factory_.get();
    }

    DCHECK(request_factory);
    return request_factory;
}

void ClientFileTransfer::getDriveList(common::FileTaskTarget target)
{
    request_consumer_proxy_->doRequest(requestFactory(target)->driveListRequest());
}

void ClientFileTransfer::getFileList(common::FileTaskTarget target, const std::string& path)
{
    request_consumer_proxy_->doRequest(requestFactory(target)->fileListRequest(path));
}

void ClientFileTransfer::createDirectory(common::FileTaskTarget target, const std::string& path)
{
    request_consumer_proxy_->doRequest(requestFactory(target)->createDirectoryRequest(path));
}

void ClientFileTransfer::rename(common::FileTaskTarget target,
                                const std::string& old_path,
                                const std::string& new_path)
{
    request_consumer_proxy_->doRequest(requestFactory(target)->renameRequest(old_path, new_path));
}

void ClientFileTransfer::remove(common::FileTaskTarget target,
                                std::shared_ptr<FileRemoveWindowProxy> remove_window_proxy,
                                const FileRemover::TaskList& items)
{
    DCHECK(!remover_);

    remover_ = std::make_unique<FileRemover>(
        ioTaskRunner(), remove_window_proxy, request_consumer_proxy_, target);

    remover_->start(items, [this]()
    {
        remover_.reset();
    });
}

void ClientFileTransfer::transfer(std::shared_ptr<FileTransferWindowProxy> transfer_window_proxy,
                                  FileTransfer::Type transfer_type,
                                  const std::string& source_path,
                                  const std::string& target_path,
                                  const std::vector<FileTransfer::Item>& items)
{
    DCHECK(!transfer_);

    transfer_ = std::make_unique<FileTransfer>(
        ioTaskRunner(), transfer_window_proxy, request_consumer_proxy_, transfer_type);

    transfer_->start(source_path, target_path, items, [this]()
    {
        transfer_.reset();
    });
}

} // namespace client
