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

#include "client/file_remover.h"

#include "base/logging.h"
#include "client/file_remove_queue_builder.h"
#include "client/file_remove_window_proxy.h"
#include "client/file_remover_proxy.h"
#include "common/file_task_factory.h"
#include "common/file_task_consumer_proxy.h"
#include "common/file_task_producer_proxy.h"

namespace client {

//--------------------------------------------------------------------------------------------------
FileRemover::FileRemover(std::shared_ptr<base::TaskRunner> io_task_runner,
                         std::shared_ptr<FileRemoveWindowProxy> remove_window_proxy,
                         std::shared_ptr<common::FileTaskConsumerProxy> task_consumer_proxy,
                         common::FileTask::Target target,
                         QObject* parent)
    : QObject(parent),
      remover_proxy_(std::make_shared<FileRemoverProxy>(std::move(io_task_runner), this)),
      remove_window_proxy_(std::move(remove_window_proxy)),
      task_consumer_proxy_(std::move(task_consumer_proxy)),
      task_producer_proxy_(std::make_shared<common::FileTaskProducerProxy>(this))
{
    LOG(LS_INFO) << "Ctor";

    DCHECK(remove_window_proxy_);
    DCHECK(task_consumer_proxy_);

    task_factory_ = std::make_unique<common::FileTaskFactory>(task_producer_proxy_, target);
}

//--------------------------------------------------------------------------------------------------
FileRemover::~FileRemover()
{
    LOG(LS_INFO) << "Dtor";
    task_producer_proxy_->dettach();
    remover_proxy_->dettach();
}

//--------------------------------------------------------------------------------------------------
void FileRemover::start(const TaskList& items, const FinishCallback& callback)
{
    LOG(LS_INFO) << "Start file remover";

    finish_callback_ = callback;

    // Asynchronously start UI.
    remove_window_proxy_->start(remover_proxy_);

    queue_builder_ = std::make_unique<FileRemoveQueueBuilder>(
        task_consumer_proxy_, task_factory_->target());

    connect(queue_builder_.get(), &FileRemoveQueueBuilder::sig_finished,
            this, [this](proto::FileError error_code)
    {
        if (error_code == proto::FILE_ERROR_SUCCESS)
        {
            tasks_ = queue_builder_->takeQueue();
            tasks_count_ = tasks_.size();

            doCurrentTask();
        }
        else
        {
            remove_window_proxy_->errorOccurred(std::string(), error_code, ACTION_ABORT);
        }

        queue_builder_.release()->deleteLater();
    });

    // Start building a list of objects for deletion.
    queue_builder_->start(items);
}

//--------------------------------------------------------------------------------------------------
void FileRemover::stop()
{
    LOG(LS_INFO) << "File remover stop";

    queue_builder_.reset();
    tasks_.clear();

    onFinished(FROM_HERE);
}

//--------------------------------------------------------------------------------------------------
void FileRemover::setAction(Action action)
{
    switch (action)
    {
        case ACTION_SKIP:
            doNextTask();
            break;

        case ACTION_SKIP_ALL:
            failure_action_ = action;
            doNextTask();
            break;

        case ACTION_ABORT:
            onFinished(FROM_HERE);
            break;

        default:
            NOTREACHED();
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void FileRemover::onTaskDone(std::shared_ptr<common::FileTask> task)
{
    const proto::FileRequest& request = task->request();
    const proto::FileReply& reply = task->reply();

    if (!request.has_remove_request())
    {
        remove_window_proxy_->errorOccurred(
            request.remove_request().path(), proto::FILE_ERROR_UNKNOWN, ACTION_ABORT);
        return;
    }

    if (reply.error_code() != proto::FILE_ERROR_SUCCESS)
    {
        uint32_t actions;

        switch (reply.error_code())
        {
            case proto::FILE_ERROR_PATH_NOT_FOUND:
            case proto::FILE_ERROR_ACCESS_DENIED:
            {
                if (failure_action_ != ACTION_ASK)
                {
                    setAction(failure_action_);
                    return;
                }

                actions = ACTION_ABORT | ACTION_SKIP | ACTION_SKIP_ALL;
            }
            break;

            default:
                actions = ACTION_ABORT;
                break;
        }

        remove_window_proxy_->errorOccurred(
            request.remove_request().path(), reply.error_code(), actions);
        return;
    }

    doNextTask();
}

//--------------------------------------------------------------------------------------------------
void FileRemover::doNextTask()
{
    // The task is completed. We delete it.
    if (!tasks_.empty())
        tasks_.pop_front();

    doCurrentTask();
}

//--------------------------------------------------------------------------------------------------
void FileRemover::doCurrentTask()
{
    if (tasks_.empty())
    {
        onFinished(FROM_HERE);
        return;
    }

    DCHECK_NE(tasks_count_, 0);

    const size_t percentage = (tasks_count_ - tasks_.size()) * 100 / tasks_count_;
    const std::string& path = tasks_.front().path();

    // Updating progress in UI.
    remove_window_proxy_->setCurrentProgress(path, static_cast<int>(percentage));

    // Send a request to delete the next item.
    task_consumer_proxy_->doTask(task_factory_->remove(path));
}

//--------------------------------------------------------------------------------------------------
void FileRemover::onFinished(const base::Location& location)
{
    LOG(LS_INFO) << "File remover finished (from: " << location.toString() << ")";

    FinishCallback callback;
    callback.swap(finish_callback_);

    if (callback)
    {
        remove_window_proxy_->stop();
        callback();
    }
}

//--------------------------------------------------------------------------------------------------
FileRemover::Task::Task(std::string&& path, bool is_directory)
    : path_(std::move(path)),
      is_directory_(is_directory)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
FileRemover::Task::Task(Task&& other) noexcept
    : path_(std::move(other.path_)),
      is_directory_(other.is_directory_)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
FileRemover::Task& FileRemover::Task::operator=(Task&& other) noexcept
{
    path_ = std::move(other.path_);
    is_directory_ = other.is_directory_;
    return *this;
}

} // namespace client
