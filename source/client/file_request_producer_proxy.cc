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

#include "client/file_request_producer_proxy.h"

#include "base/task_runner.h"
#include "client/file_request_producer.h"

namespace client {

FileRequestProducerProxy::FileRequestProducerProxy(
    const std::weak_ptr<FileRequestProducer>& request_producer,
    std::unique_ptr<base::TaskRunner> task_runner)
    : request_producer_(request_producer),
      task_runner_(std::move(task_runner))
{
    // Nothing
}

FileRequestProducerProxy::~FileRequestProducerProxy() = default;

void FileRequestProducerProxy::onLocalReply(const proto::FileReply& reply)
{
    if (!task_runner_->belongsToCurrentThread())
    {
        task_runner_->postTask(std::bind(&FileRequestProducerProxy::onLocalReply, this, reply));
        return;
    }

    std::shared_ptr<FileRequestProducer> request_producer = request_producer_.lock();
    if (request_producer)
        request_producer->onLocalReply(reply);
}

void FileRequestProducerProxy::onRemoteReply(const proto::FileReply& reply)
{
    if (!task_runner_->belongsToCurrentThread())
    {
        task_runner_->postTask(std::bind(&FileRequestProducerProxy::onRemoteReply, this, reply));
        return;
    }

    std::shared_ptr<FileRequestProducer> request_producer = request_producer_.lock();
    if (request_producer)
        request_producer->onRemoteReply(reply);
}

} // namespace client
