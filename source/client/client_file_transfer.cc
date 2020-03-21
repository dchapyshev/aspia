//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "common/file_task_factory.h"
#include "common/file_task_consumer_proxy.h"
#include "common/file_task_producer_proxy.h"
#include "common/file_worker.h"

namespace client {

ClientFileTransfer::ClientFileTransfer(std::shared_ptr<base::TaskRunner> ui_task_runner)
    : Client(std::move(ui_task_runner))
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
    task_consumer_proxy_ = std::make_shared<common::FileTaskConsumerProxy>(this);
    task_producer_proxy_ = std::make_shared<common::FileTaskProducerProxy>(this);

    local_task_factory_ = std::make_unique<common::FileTaskFactory>(
        task_producer_proxy_, common::FileTask::Target::LOCAL);

    remote_task_factory_ = std::make_unique<common::FileTaskFactory>(
        task_producer_proxy_, common::FileTask::Target::REMOTE);

    file_control_proxy_ = std::make_shared<FileControlProxy>(ioTaskRunner(), this);
    local_worker_ = std::make_unique<common::FileWorker>(ioTaskRunner());

    file_manager_window_proxy_->start(file_control_proxy_);
}

void ClientFileTransfer::onSessionStopped()
{
    task_consumer_proxy_->dettach();
    task_producer_proxy_->dettach();
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

    if (reply->error_code() == proto::FILE_ERROR_NO_LOGGED_ON_USER)
    {
        file_manager_window_proxy_->onErrorOccurred(reply->error_code());
    }
    else if (!remote_task_queue_.empty())
    {
        // Move the reply to the request and notify the sender.
        remote_task_queue_.front()->setReply(std::move(reply));

        // Remove the request from the queue.
        remote_task_queue_.pop();

        // Execute the next request.
        doNextRemoteTask();
    }
    else
    {
        file_manager_window_proxy_->onErrorOccurred(proto::FILE_ERROR_UNKNOWN);
    }
}

void ClientFileTransfer::onMessageWritten()
{
    // Nothing
}

void ClientFileTransfer::onTaskDone(std::shared_ptr<common::FileTask> task)
{
    const proto::FileRequest& request = task->request();
    const proto::FileReply& reply = task->reply();

    if (request.has_drive_list_request())
    {
        file_manager_window_proxy_->onDriveList(
            task->target(), reply.error_code(), reply.drive_list());
    }
    else if (request.has_file_list_request())
    {
        file_manager_window_proxy_->onFileList(
            task->target(), reply.error_code(), reply.file_list());
    }
    else if (request.has_create_directory_request())
    {
        file_manager_window_proxy_->onCreateDirectory(task->target(), reply.error_code());
    }
    else if (request.has_rename_request())
    {
        file_manager_window_proxy_->onRename(task->target(), reply.error_code());
    }
    else
    {
        NOTREACHED();
    }
}

void ClientFileTransfer::doTask(std::shared_ptr<common::FileTask> task)
{
    if (task->target() == common::FileTask::Target::LOCAL)
    {
        local_worker_->doTask(std::move(task));
    }
    else
    {
        const bool schedule = remote_task_queue_.empty();

        // Add the request to the queue.
        remote_task_queue_.emplace(std::move(task));

        // If the request queue was empty, then run execution.
        if (schedule)
            doNextRemoteTask();
    }
}

void ClientFileTransfer::doNextRemoteTask()
{
    if (remote_task_queue_.empty())
        return;

    // Send a request to the remote computer.
    sendMessage(remote_task_queue_.front()->request());
}

common::FileTaskFactory* ClientFileTransfer::taskFactory(common::FileTask::Target target)
{
    common::FileTaskFactory* task_factory;

    if (target == common::FileTask::Target::LOCAL)
    {
        task_factory = local_task_factory_.get();
    }
    else
    {
        DCHECK_EQ(target, common::FileTask::Target::REMOTE);
        task_factory = remote_task_factory_.get();
    }

    DCHECK(task_factory);
    return task_factory;
}

void ClientFileTransfer::driveList(common::FileTask::Target target)
{
    task_consumer_proxy_->doTask(taskFactory(target)->driveList());
}

void ClientFileTransfer::fileList(common::FileTask::Target target, const std::string& path)
{
    task_consumer_proxy_->doTask(taskFactory(target)->fileList(path));
}

void ClientFileTransfer::createDirectory(common::FileTask::Target target, const std::string& path)
{
    task_consumer_proxy_->doTask(taskFactory(target)->createDirectory(path));
}

void ClientFileTransfer::rename(common::FileTask::Target target,
                                const std::string& old_path,
                                const std::string& new_path)
{
    task_consumer_proxy_->doTask(taskFactory(target)->rename(old_path, new_path));
}

void ClientFileTransfer::remove(common::FileTask::Target target,
                                std::shared_ptr<FileRemoveWindowProxy> remove_window_proxy,
                                const FileRemover::TaskList& items)
{
    DCHECK(!remover_);

    remover_ = std::make_unique<FileRemover>(
        ioTaskRunner(), remove_window_proxy, task_consumer_proxy_, target);

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
        ioTaskRunner(), transfer_window_proxy, task_consumer_proxy_, transfer_type);

    transfer_->start(source_path, target_path, items, [this]()
    {
        transfer_.reset();
    });
}

} // namespace client
