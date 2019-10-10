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

#include "client/file_remove_queue_builder.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "client/file_request_factory.h"
#include "common/file_request.h"
#include "common/file_request_consumer_proxy.h"
#include "common/file_request_producer_proxy.h"

namespace client {

FileRemoveQueueBuilder::FileRemoveQueueBuilder(
    std::shared_ptr<common::FileRequestConsumerProxy> request_consumer_proxy,
    common::FileTaskTarget target)
    : request_consumer_proxy_(request_consumer_proxy),
      request_producer_proxy_(std::make_shared<common::FileRequestProducerProxy>(this))
{
    DCHECK(request_consumer_proxy_);

    request_factory_ = std::make_unique<FileRequestFactory>(request_producer_proxy_, target);
}

FileRemoveQueueBuilder::~FileRemoveQueueBuilder()
{
    request_producer_proxy_->dettach();
}

void FileRemoveQueueBuilder::start(const FileRemover::TaskList& items, FinishCallback callback)
{
    pending_tasks_ = items;

    callback_ = std::move(callback);
    DCHECK(callback_);

    doPendingTasks();
}

FileRemover::TaskList FileRemoveQueueBuilder::takeQueue()
{
    return std::move(tasks_);
}

void FileRemoveQueueBuilder::onReply(std::shared_ptr<common::FileRequest> request)
{
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

    const std::string& path = file_request.file_list_request().path();

    for (int i = 0; i < file_reply.file_list().item_size(); ++i)
    {
        const proto::FileList::Item& item = file_reply.file_list().item(i);
        std::string item_path = path + '/' + item.name();

        pending_tasks_.emplace_back(std::move(item_path), item.is_directory());
    }

    doPendingTasks();
}

void FileRemoveQueueBuilder::doPendingTasks()
{
    while (!pending_tasks_.empty())
    {
        tasks_.emplace_front(std::move(pending_tasks_.front()));
        pending_tasks_.pop_front();

        if (tasks_.front().isDirectory())
        {
            request_consumer_proxy_->doRequest(
                request_factory_->fileListRequest(tasks_.front().path()));
            return;
        }
    }

    callback_(proto::FILE_ERROR_SUCCESS);
}

void FileRemoveQueueBuilder::onAborted(proto::FileError error_code)
{
    pending_tasks_.clear();
    tasks_.clear();

    callback_(error_code);
}

} // namespace client
