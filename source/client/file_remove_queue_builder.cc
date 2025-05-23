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

namespace client {

//--------------------------------------------------------------------------------------------------
FileRemoveQueueBuilder::FileRemoveQueueBuilder(common::FileTask::Target target, QObject* parent)
    : QObject(parent),
      task_factory_(std::make_unique<common::FileTaskFactory>(target))
{
    LOG(LS_INFO) << "Ctor";

    connect(task_factory_.get(), &common::FileTaskFactory::sig_taskDone,
            this, &FileRemoveQueueBuilder::onTaskDone);
}

//--------------------------------------------------------------------------------------------------
FileRemoveQueueBuilder::~FileRemoveQueueBuilder()
{
    LOG(LS_INFO) << "Dtor";
    task_factory_.reset();
}

//--------------------------------------------------------------------------------------------------
void FileRemoveQueueBuilder::start(const FileRemover::TaskList& items)
{
    LOG(LS_INFO) << "Start remove queue builder";
    pending_tasks_ = items;
    doPendingTasks();
}

//--------------------------------------------------------------------------------------------------
FileRemover::TaskList FileRemoveQueueBuilder::takeQueue()
{
    return std::move(tasks_);
}

//--------------------------------------------------------------------------------------------------
void FileRemoveQueueBuilder::onTaskDone(const common::FileTask& task)
{
    const proto::FileRequest& request = task.request();
    const proto::FileReply& reply = task.reply();

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

    const QString& path = QString::fromStdString(request.file_list_request().path());

    for (int i = 0; i < reply.file_list().item_size(); ++i)
    {
        const proto::FileList::Item& item = reply.file_list().item(i);
        QString item_path = path + '/' + QString::fromStdString(item.name());

        pending_tasks_.push_back(FileRemover::Task(std::move(item_path), item.is_directory()));
    }

    doPendingTasks();
}

//--------------------------------------------------------------------------------------------------
void FileRemoveQueueBuilder::doPendingTasks()
{
    while (!pending_tasks_.empty())
    {
        tasks_.push_front(std::move(pending_tasks_.front()));
        pending_tasks_.pop_front();

        if (tasks_.front().isDirectory())
        {
            emit sig_doTask(task_factory_->fileList(tasks_.front().path()));
            return;
        }
    }

    emit sig_finished(proto::FILE_ERROR_SUCCESS);
}

//--------------------------------------------------------------------------------------------------
void FileRemoveQueueBuilder::onAborted(proto::FileError error_code)
{
    LOG(LS_INFO) << "Aborted: " << error_code;

    pending_tasks_.clear();
    tasks_.clear();

    emit sig_finished(error_code);
}

} // namespace client
