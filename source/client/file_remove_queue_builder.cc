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

#include "client/file_remove_queue_builder.h"

#include "base/logging.h"
#include "common/file_task_factory.h"
#include "common/file_task_consumer_proxy.h"
#include "common/file_task_producer_proxy.h"

namespace client {

//--------------------------------------------------------------------------------------------------
FileRemoveQueueBuilder::FileRemoveQueueBuilder(
    std::shared_ptr<common::FileTaskConsumerProxy> task_consumer_proxy,
    common::FileTask::Target target,
    QObject* parent)
    : QObject(parent),
      task_consumer_proxy_(std::move(task_consumer_proxy)),
      task_producer_proxy_(std::make_shared<common::FileTaskProducerProxy>(this))
{
    LOG(LS_INFO) << "Ctor";
    DCHECK(task_consumer_proxy_);

    task_factory_ = std::make_unique<common::FileTaskFactory>(task_producer_proxy_, target);
}

//--------------------------------------------------------------------------------------------------
FileRemoveQueueBuilder::~FileRemoveQueueBuilder()
{
    LOG(LS_INFO) << "Dtor";
    task_producer_proxy_->dettach();
}

//--------------------------------------------------------------------------------------------------
void FileRemoveQueueBuilder::start(
    const FileRemover::TaskList& items, const FinishCallback& callback)
{
    LOG(LS_INFO) << "Start remove queue builder";

    pending_tasks_ = items;
    callback_ = callback;

    DCHECK(callback_);

    doPendingTasks();
}

//--------------------------------------------------------------------------------------------------
FileRemover::TaskList FileRemoveQueueBuilder::takeQueue()
{
    return std::move(tasks_);
}

//--------------------------------------------------------------------------------------------------
void FileRemoveQueueBuilder::onTaskDone(std::shared_ptr<common::FileTask> task)
{
    const proto::FileRequest& request = task->request();
    const proto::FileReply& reply = task->reply();

    if (!request.has_file_list_request())
    {
        onAborted(proto::FILE_ERROR_UNKNOWN);
        return;
    }

    if (reply.error_code() != proto::FILE_ERROR_SUCCESS)
    {
        onAborted(reply.error_code());
        return;
    }

    const std::string& path = request.file_list_request().path();

    for (int i = 0; i < reply.file_list().item_size(); ++i)
    {
        const proto::FileList::Item& item = reply.file_list().item(i);
        std::string item_path = path + '/' + item.name();

        pending_tasks_.emplace_back(std::move(item_path), item.is_directory());
    }

    doPendingTasks();
}

//--------------------------------------------------------------------------------------------------
void FileRemoveQueueBuilder::doPendingTasks()
{
    while (!pending_tasks_.empty())
    {
        tasks_.emplace_front(std::move(pending_tasks_.front()));
        pending_tasks_.pop_front();

        if (tasks_.front().isDirectory())
        {
            task_consumer_proxy_->doTask(task_factory_->fileList(tasks_.front().path()));
            return;
        }
    }

    callback_(proto::FILE_ERROR_SUCCESS);
}

//--------------------------------------------------------------------------------------------------
void FileRemoveQueueBuilder::onAborted(proto::FileError error_code)
{
    LOG(LS_INFO) << "Aborted: " << error_code;

    pending_tasks_.clear();
    tasks_.clear();

    callback_(error_code);
}

} // namespace client
