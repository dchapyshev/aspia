//
// SmartCafe Project
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
#include "common/file_task_factory.h"

namespace common {

//--------------------------------------------------------------------------------------------------
FileTask::FileTask(QPointer<FileTaskFactory> factory,
                   proto::file_transfer::Request&& request,
                   Target target)
    : data_(new Data(factory, std::move(request), target))
{
    DCHECK(data_->factory);
    DCHECK(data_->target == Target::LOCAL || data_->target == Target::REMOTE);
}

//--------------------------------------------------------------------------------------------------
FileTask::~FileTask() = default;

//--------------------------------------------------------------------------------------------------
FileTask::Target FileTask::target() const
{
    return data_->target;
}

//--------------------------------------------------------------------------------------------------
const proto::file_transfer::Request& FileTask::request() const
{
    return data_->request;
}

//--------------------------------------------------------------------------------------------------
const proto::file_transfer::Reply& FileTask::reply() const
{
    return data_->reply;
}

//--------------------------------------------------------------------------------------------------
void FileTask::onReply(proto::file_transfer::Reply&& reply)
{
    if (!data_->factory)
        return;

    // Save the reply inside the request.
    data_->reply = std::move(reply);

    // Now notify the sender of the reply.
    emit data_->factory->sig_taskDone(*this);
}

} // namespace common
