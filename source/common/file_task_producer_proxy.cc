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

#include "common/file_task_producer_proxy.h"

#include "base/logging.h"

namespace common {

//--------------------------------------------------------------------------------------------------
FileTaskProducerProxy::FileTaskProducerProxy(FileTaskProducer* task_producer)
    : task_producer_(task_producer)
{
    DCHECK(task_producer_);
}

//--------------------------------------------------------------------------------------------------
FileTaskProducerProxy::~FileTaskProducerProxy()
{
    DCHECK(!task_producer_);
}

//--------------------------------------------------------------------------------------------------
void FileTaskProducerProxy::dettach()
{
    task_producer_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void FileTaskProducerProxy::onTaskDone(std::shared_ptr<FileTask> task)
{
    if (task_producer_)
        task_producer_->onTaskDone(task);
}

} // namespace common
