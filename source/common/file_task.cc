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

#include "common/file_task.h"

#include "base/logging.h"
#include "common/file_task_producer_proxy.h"
#include "proto/file_transfer.h"

namespace common {

//--------------------------------------------------------------------------------------------------
FileTask::FileTask(std::shared_ptr<FileTaskProducerProxy> producer_proxy,
                   std::unique_ptr<proto::FileRequest> request,
                   Target target)
    : producer_proxy_(std::move(producer_proxy)),
      target_(target),
      request_(std::move(request))
{
    DCHECK(producer_proxy_);
    DCHECK(request_);
    DCHECK(target_ == Target::LOCAL || target_ == Target::REMOTE);
}

//--------------------------------------------------------------------------------------------------
FileTask::~FileTask() = default;

//--------------------------------------------------------------------------------------------------
const proto::FileRequest& FileTask::request() const
{
    return *request_;
}

//--------------------------------------------------------------------------------------------------
const proto::FileReply& FileTask::reply() const
{
    static const proto::FileReply kEmptyReply;

    if (!reply_)
        return kEmptyReply;

    return *reply_;
}

//--------------------------------------------------------------------------------------------------
void FileTask::setReply(std::unique_ptr<proto::FileReply> reply)
{
    // Save the reply inside the request.
    reply_ = std::move(reply);

    // Now notify the sender of the reply.
    producer_proxy_->onTaskDone(shared_from_this());
}

} // namespace common
