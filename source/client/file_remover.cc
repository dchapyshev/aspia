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

#include "client/file_remover.h"

#include "base/logging.h"
#include "client/file_remove_queue_builder.h"
#include "client/file_remove_window_proxy.h"
#include "client/file_remover_proxy.h"
#include "client/file_request_factory.h"
#include "common/file_request.h"
#include "common/file_request_consumer_proxy.h"
#include "common/file_request_producer_proxy.h"

namespace client {

FileRemover::FileRemover(std::shared_ptr<base::TaskRunner>& io_task_runner,
                         std::shared_ptr<FileRemoveWindowProxy>& remove_window_proxy,
                         std::shared_ptr<common::FileRequestConsumerProxy>& request_consumer_proxy,
                         common::FileTaskTarget target)
    : remover_proxy_(std::make_shared<FileRemoverProxy>(io_task_runner, this)),
      remove_window_proxy_(remove_window_proxy),
      request_consumer_proxy_(request_consumer_proxy),
      request_producer_proxy_(std::make_shared<common::FileRequestProducerProxy>(this))
{
    DCHECK(remove_window_proxy_);
    DCHECK(request_consumer_proxy_);

    request_factory_ = std::make_unique<FileRequestFactory>(request_producer_proxy_, target);
}

FileRemover::~FileRemover()
{
    request_producer_proxy_->dettach();
    remover_proxy_->dettach();
}

void FileRemover::start(const TaskList& items, const FinishCallback& callback)
{
    finish_callback_ = callback;

    // Asynchronously start UI.
    remove_window_proxy_->start(remover_proxy_);

    queue_builder_ = std::make_unique<FileRemoveQueueBuilder>(
        request_consumer_proxy_, request_factory_->target());

    // Start building a list of objects for deletion.
    queue_builder_->start(items, [this](proto::FileError error_code)
    {
        if (error_code == proto::FILE_ERROR_SUCCESS)
        {
            tasks_ = queue_builder_->takeQueue();
            tasks_count_ = tasks_.size();

            doNextTask();
        }
        else
        {
            remove_window_proxy_->errorOccurred(std::string(), error_code, ACTION_ABORT);
        }

        queue_builder_.reset();
    });
}

void FileRemover::stop()
{
    queue_builder_.reset();
    tasks_.clear();

    onFinished();
}

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
            onFinished();
            break;

        default:
            NOTREACHED();
            break;
    }
}

void FileRemover::onReply(std::shared_ptr<common::FileRequest> request)
{
    const proto::FileRequest& file_request = request->request();
    const proto::FileReply& file_reply = request->reply();

    if (!file_request.has_remove_request())
    {
        remove_window_proxy_->errorOccurred(
            file_request.remove_request().path(), proto::FILE_ERROR_UNKNOWN, ACTION_ABORT);
        return;
    }

    if (file_reply.error_code() != proto::FILE_ERROR_SUCCESS)
    {
        uint32_t actions;

        switch (file_reply.error_code())
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
            file_request.remove_request().path(), file_reply.error_code(), actions);
        return;
    }

    // The task is completed. We delete it.
    if (!tasks_.empty())
        tasks_.pop_front();

    doNextTask();
}

void FileRemover::doNextTask()
{
    if (tasks_.empty())
    {
        onFinished();
        return;
    }

    DCHECK_NE(tasks_count_, 0);

    const size_t percentage = (tasks_count_ - tasks_.size()) * 100 / tasks_count_;
    const std::string& path = tasks_.front().path();

    // Updating progress in UI.
    remove_window_proxy_->setCurrentProgress(path, percentage);

    // Send a request to delete the next item.
    request_consumer_proxy_->doRequest(request_factory_->removeRequest(path));
}

void FileRemover::onFinished()
{
    FinishCallback callback;
    callback.swap(finish_callback_);

    if (callback)
    {
        remove_window_proxy_->stop();
        callback();
    }
}

FileRemover::Task::Task(std::string&& path, bool is_directory)
    : path_(std::move(path)),
      is_directory_(is_directory)
{
    // Nothing
}

FileRemover::Task::Task(Task&& other) noexcept
    : path_(std::move(other.path_)),
      is_directory_(other.is_directory_)
{
    // Nothing
}

FileRemover::Task& FileRemover::Task::operator=(Task&& other) noexcept
{
    path_ = std::move(other.path_);
    is_directory_ = other.is_directory_;
    return *this;
}

} // namespace client
