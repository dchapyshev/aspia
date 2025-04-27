//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "base/task_runner.h"
#include "client/file_control_proxy.h"
#include "client/file_manager_window_proxy.h"
#include "common/file_task_factory.h"
#include "common/file_task_consumer_proxy.h"
#include "common/file_task_producer_proxy.h"
#include "common/file_worker.h"
#include "qt_base/application.h"

namespace client {

//--------------------------------------------------------------------------------------------------
ClientFileTransfer::ClientFileTransfer(std::shared_ptr<base::TaskRunner> io_task_runner, QObject* parent)
    : Client(io_task_runner, parent),
      task_consumer_proxy_(std::make_shared<common::FileTaskConsumerProxy>(this)),
      task_producer_proxy_(std::make_shared<common::FileTaskProducerProxy>(this)),
      local_worker_(std::make_unique<common::FileWorker>(io_task_runner)),
      file_control_proxy_(std::make_shared<FileControlProxy>(io_task_runner, this))
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
ClientFileTransfer::~ClientFileTransfer()
{
    LOG(LS_INFO) << "Dtor";

    task_consumer_proxy_->dettach();
    task_producer_proxy_->dettach();
    file_control_proxy_->dettach();

    remover_.reset();
    transfer_.reset();
}

//--------------------------------------------------------------------------------------------------
void ClientFileTransfer::setFileManagerWindow(
    std::shared_ptr<FileManagerWindowProxy> file_manager_window_proxy)
{
    LOG(LS_INFO) << "File transfer window installed";
    file_manager_window_proxy_ = std::move(file_manager_window_proxy);
}

//--------------------------------------------------------------------------------------------------
void ClientFileTransfer::onSessionStarted()
{
    LOG(LS_INFO) << "File transfer session started";

    local_task_factory_ = std::make_unique<common::FileTaskFactory>(
        task_producer_proxy_, common::FileTask::Target::LOCAL);

    remote_task_factory_ = std::make_unique<common::FileTaskFactory>(
        task_producer_proxy_, common::FileTask::Target::REMOTE);

    file_manager_window_proxy_->start(file_control_proxy_);
}

//--------------------------------------------------------------------------------------------------
void ClientFileTransfer::onSessionMessageReceived(uint8_t /* channel_id */, const QByteArray& buffer)
{
    std::unique_ptr<proto::FileReply> reply = std::make_unique<proto::FileReply>();

    if (!reply->ParseFromArray(buffer.data(), static_cast<int>(buffer.size())))
    {
        LOG(LS_ERROR) << "Invalid message from host";
        return;
    }

    if (reply->error_code() == proto::FILE_ERROR_NO_LOGGED_ON_USER)
    {
        LOG(LS_INFO) << "No logged in user on host side";
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

//--------------------------------------------------------------------------------------------------
void ClientFileTransfer::onSessionMessageWritten(uint8_t /* channel_id */, size_t /* pending */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------------------
void ClientFileTransfer::doNextRemoteTask()
{
    if (remote_task_queue_.empty())
        return;

    // Send a request to the remote computer.
    sendMessage(proto::HOST_CHANNEL_ID_SESSION, remote_task_queue_.front()->request());
}

//--------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------------------
void ClientFileTransfer::driveList(common::FileTask::Target target)
{
    task_consumer_proxy_->doTask(taskFactory(target)->driveList());
}

//--------------------------------------------------------------------------------------------------
void ClientFileTransfer::fileList(common::FileTask::Target target, const std::string& path)
{
    task_consumer_proxy_->doTask(taskFactory(target)->fileList(path));
}

//--------------------------------------------------------------------------------------------------
void ClientFileTransfer::createDirectory(common::FileTask::Target target, const std::string& path)
{
    task_consumer_proxy_->doTask(taskFactory(target)->createDirectory(path));
}

//--------------------------------------------------------------------------------------------------
void ClientFileTransfer::rename(common::FileTask::Target target,
                                const std::string& old_path,
                                const std::string& new_path)
{
    task_consumer_proxy_->doTask(taskFactory(target)->rename(old_path, new_path));
}

//--------------------------------------------------------------------------------------------------
void ClientFileTransfer::remove(FileRemover* remover)
{
    DCHECK(!remover_);
    remover_.reset(remover);

    connect(remover_.get(), &FileRemover::sig_finished, this, [this]()
    {
        remover_.release()->deleteLater();
    });

    remover_->moveToThread(base::GuiApplication::ioThread());
    remover_->start(task_consumer_proxy_);
}

//--------------------------------------------------------------------------------------------------
void ClientFileTransfer::transfer(FileTransfer* transfer)
{
    DCHECK(!transfer_);
    transfer_.reset(transfer);

    connect(transfer_.get(), &FileTransfer::sig_finished, this, [this]()
    {
        transfer_.release()->deleteLater();
    });

    transfer_->moveToThread(base::GuiApplication::ioThread());
    transfer_->start(task_consumer_proxy_);
}

} // namespace client
