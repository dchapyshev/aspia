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

#include "client/file_request_consumer_proxy.h"

#include "base/task_runner.h"
#include "client/file_request_consumer.h"

namespace client {

FileRequestConsumerProxy::FileRequestConsumerProxy(
    const std::weak_ptr<FileRequestConsumer>& request_consumer,
    std::unique_ptr<base::TaskRunner> task_runner)
    : request_consumer_(request_consumer),
      task_runner_(std::move(task_runner))
{
    // Nothing
}

FileRequestConsumerProxy::~FileRequestConsumerProxy() = default;

void FileRequestConsumerProxy::localRequest(const proto::FileRequest& request)
{
    if (!task_runner_->belongsToCurrentThread())
    {
        task_runner_->postTask(std::bind(&FileRequestConsumerProxy::localRequest, this, request));
        return;
    }

    std::shared_ptr<FileRequestConsumer> request_consumer = request_consumer_.lock();
    if (request_consumer)
        request_consumer->localRequest(request);
}

void FileRequestConsumerProxy::remoteRequest(const proto::FileRequest& request)
{
    if (!task_runner_->belongsToCurrentThread())
    {
        task_runner_->postTask(std::bind(&FileRequestConsumerProxy::remoteRequest, this, request));
        return;
    }

    std::shared_ptr<FileRequestConsumer> request_consumer = request_consumer_.lock();
    if (request_consumer)
        request_consumer->remoteRequest(request);
}

} // namespace client
