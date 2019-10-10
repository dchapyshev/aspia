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

#include "client/file_transfer_queue_builder.h"

#include "base/logging.h"
#include "client/file_request_factory.h"
#include "common/file_request.h"
#include "common/file_request_consumer_proxy.h"
#include "common/file_request_producer_proxy.h"

namespace client {

FileTransferQueueBuilder::FileTransferQueueBuilder(
    std::shared_ptr<common::FileRequestConsumerProxy>& request_consumer_proxy,
    common::FileTaskTarget target)
    : request_consumer_proxy_(request_consumer_proxy),
      request_producer_proxy_(std::make_shared<common::FileRequestProducerProxy>(this))
{
    DCHECK(request_consumer_proxy_);

    request_factory_ = std::make_unique<FileRequestFactory>(request_producer_proxy_, target);
}

FileTransferQueueBuilder::~FileTransferQueueBuilder()
{
    request_producer_proxy_->dettach();
}

void FileTransferQueueBuilder::start(const std::string& source_path,
                                     const std::string& target_path,
                                     const std::vector<FileTransfer::Item>& items,
                                     const FinishCallback& callback)
{
    callback_ = callback;
    DCHECK(callback_);

    for (const auto& item : items)
        addPendingTask(source_path, target_path, item.name, item.is_directory, item.size);

    doPendingTasks();
}

FileTransfer::TaskList FileTransferQueueBuilder::takeQueue()
{
    return std::move(tasks_);
}

int64_t FileTransferQueueBuilder::totalSize() const
{
    return total_size_;
}

void FileTransferQueueBuilder::onReply(std::shared_ptr<common::FileRequest> request)
{
    DCHECK(!tasks_.empty());

    const proto::FileRequest& file_request = request->request();
    const proto::FileReply& file_reply = request->reply();

    if (!file_request.has_file_list_request())
    {
        onAborted(proto::FILE_ERROR_UNKNOWN);
        return;
    }

    if (file_reply.error_code() != proto::FILE_ERROR_SUCCESS)
    {
        onAborted(file_reply.error_code());
        return;
    }

    // If we get a list of files, then the last task is a directory.
    const FileTransferTask& last_task = tasks_.back();
    DCHECK(last_task.isDirectory());

    for (int i = 0; i < file_reply.file_list().item_size(); ++i)
    {
        const proto::FileList::Item& item = file_reply.file_list().item(i);

        addPendingTask(last_task.sourcePath(),
                       last_task.targetPath(),
                       item.name(),
                       item.is_directory(),
                       item.size());
    }

    doPendingTasks();
}

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

void FileTransferQueueBuilder::doPendingTasks()
{
    while (!pending_tasks_.empty())
    {
        tasks_.emplace_back(std::move(pending_tasks_.front()));
        pending_tasks_.pop_front();

        if (tasks_.back().isDirectory())
        {
            request_consumer_proxy_->doRequest(
                request_factory_->fileListRequest(tasks_.back().sourcePath()));
            return;
        }
    }

    callback_(proto::FILE_ERROR_SUCCESS);
}

void FileTransferQueueBuilder::onAborted(proto::FileError error_code)
{
    pending_tasks_.clear();
    tasks_.clear();
    total_size_ = 0;

    callback_(error_code);
}

} // namespace client
