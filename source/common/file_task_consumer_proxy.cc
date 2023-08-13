//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "common/file_task_consumer_proxy.h"

#include "base/logging.h"

namespace common {

//--------------------------------------------------------------------------------------------------
FileTaskConsumerProxy::FileTaskConsumerProxy(FileTaskConsumer* task_consumer)
    : task_consumer_(task_consumer)
{
    DCHECK(task_consumer_);
}

//--------------------------------------------------------------------------------------------------
FileTaskConsumerProxy::~FileTaskConsumerProxy()
{
    DCHECK(!task_consumer_);
}

//--------------------------------------------------------------------------------------------------
void FileTaskConsumerProxy::dettach()
{
    task_consumer_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void FileTaskConsumerProxy::doTask(std::shared_ptr<FileTask> task)
{
    if (task_consumer_)
        task_consumer_->doTask(task);
}

} // namespace common
