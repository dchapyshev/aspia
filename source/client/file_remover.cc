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
#include "common/file_task_factory.h"

namespace client {

namespace {

auto g_actionType = qRegisterMetaType<client::FileRemover::Action>();

} // namespace

//--------------------------------------------------------------------------------------------------
FileRemover::FileRemover(common::FileTask::Target target, const TaskList& items, QObject* parent)
    : QObject(parent),
      task_factory_(new common::FileTaskFactory(target, this)),
      tasks_(items)
{
    LOG(LS_INFO) << "Ctor";

    connect(task_factory_, &common::FileTaskFactory::sig_taskDone, this, &FileRemover::onTaskDone);
}

//--------------------------------------------------------------------------------------------------
FileRemover::~FileRemover()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void FileRemover::start()
{
    LOG(LS_INFO) << "Start file remover";

    // Asynchronously start UI.
    emit sig_started();

    queue_builder_ = new FileRemoveQueueBuilder(task_factory_->target(), this);

    connect(queue_builder_, &FileRemoveQueueBuilder::sig_doTask, this, &FileRemover::sig_doTask);
    connect(queue_builder_, &FileRemoveQueueBuilder::sig_finished,
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
            emit sig_errorOccurred(std::string(), error_code, ACTION_ABORT);
        }

        queue_builder_->deleteLater();
    });

    // Start building a list of objects for deletion.
    queue_builder_->start(tasks_);
}

//--------------------------------------------------------------------------------------------------
void FileRemover::stop()
{
    LOG(LS_INFO) << "File remover stop";

    delete queue_builder_;
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
void FileRemover::onTaskDone(base::local_shared_ptr<common::FileTask> task)
{
    const proto::FileRequest& request = task->request();
    const proto::FileReply& reply = task->reply();

    if (!request.has_remove_request())
    {
        emit sig_errorOccurred(
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

        emit sig_errorOccurred(request.remove_request().path(), reply.error_code(), actions);
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
    emit sig_progressChanged(path, static_cast<int>(percentage));

    // Send a request to delete the next item.
    emit sig_doTask(task_factory_->remove(path));
}

//--------------------------------------------------------------------------------------------------
void FileRemover::onFinished(const base::Location& location)
{
    LOG(LS_INFO) << "File remover finished (from: " << location.toString() << ")";
    emit sig_finished();
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
