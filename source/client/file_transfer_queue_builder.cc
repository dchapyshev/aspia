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

#include "client/file_transfer_queue_builder.h"

#include "base/logging.h"
#include "common/file_task_factory.h"

namespace client {

//--------------------------------------------------------------------------------------------------
FileTransferQueueBuilder::FileTransferQueueBuilder(common::FileTask::Target target, QObject* parent)
    : QObject(parent),
      task_factory_(std::make_unique<common::FileTaskFactory>(target))
{
    LOG(LS_INFO) << "Ctor";

    connect(task_factory_.get(), &common::FileTaskFactory::sig_taskDone,
            this, &FileTransferQueueBuilder::onTaskDone);
}

//--------------------------------------------------------------------------------------------------
FileTransferQueueBuilder::~FileTransferQueueBuilder()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void FileTransferQueueBuilder::start(const std::string& source_path,
                                     const std::string& target_path,
                                     const std::vector<FileTransfer::Item>& items)
{
    LOG(LS_INFO) << "Start file transfer queue builder";

    for (const auto& item : items)
        addPendingTask(source_path, target_path, item.name, item.is_directory, item.size);

    doPendingTasks();
}

//--------------------------------------------------------------------------------------------------
FileTransfer::TaskList FileTransferQueueBuilder::takeQueue()
{
    return std::move(tasks_);
}

//--------------------------------------------------------------------------------------------------
int64_t FileTransferQueueBuilder::totalSize() const
{
    return total_size_;
}

//--------------------------------------------------------------------------------------------------
void FileTransferQueueBuilder::onTaskDone(base::local_shared_ptr<common::FileTask> task)
{
    DCHECK(!tasks_.empty());

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

    // If we get a list of files, then the last task is a directory.
    const FileTransfer::Task& last_task = tasks_.back();
    DCHECK(last_task.isDirectory());

    for (int i = 0; i < reply.file_list().item_size(); ++i)
    {
        const proto::FileList::Item& item = reply.file_list().item(i);

        addPendingTask(last_task.sourcePath(),
                       last_task.targetPath(),
                       item.name(),
                       item.is_directory(),
                       static_cast<int64_t>(item.size()));
    }

    doPendingTasks();
}

//--------------------------------------------------------------------------------------------------
void FileTransferQueueBuilder::addPendingTask(const std::string& source_dir,
                                              const std::string& target_dir,
                                              const std::string& item_name,
                                              bool is_directory,
                                              int64_t size)
{
    total_size_ += size;

    std::string source_path = source_dir + '/' + item_name;
    std::string target_path = target_dir + '/' + item_name;

    pending_tasks_.emplace_back(std::move(source_path), std::move(target_path), is_directory, size);
}

//--------------------------------------------------------------------------------------------------
void FileTransferQueueBuilder::doPendingTasks()
{
    while (!pending_tasks_.empty())
    {
        tasks_.emplace_back(std::move(pending_tasks_.front()));
        pending_tasks_.pop_front();

        if (tasks_.back().isDirectory())
        {
            emit sig_doTask(task_factory_->fileList(tasks_.back().sourcePath()));
            return;
        }
    }

    emit sig_finished(proto::FILE_ERROR_SUCCESS);
}

//--------------------------------------------------------------------------------------------------
void FileTransferQueueBuilder::onAborted(proto::FileError error_code)
{
    LOG(LS_INFO) << "Aborted: " << error_code;

    pending_tasks_.clear();
    tasks_.clear();
    total_size_ = 0;

    emit sig_finished(error_code);
}

} // namespace client
