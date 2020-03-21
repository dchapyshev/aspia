//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/file_remover_proxy.h"

#include "base/logging.h"
#include "base/task_runner.h"

namespace client {

FileRemoverProxy::FileRemoverProxy(
    std::shared_ptr<base::TaskRunner> io_task_runner, FileRemover* remover)
    : io_task_runner_(std::move(io_task_runner)),
      remover_(remover)
{
    DCHECK(io_task_runner_);
    DCHECK(io_task_runner_->belongsToCurrentThread());
    DCHECK(remover_);
}

FileRemoverProxy::~FileRemoverProxy() = default;

void FileRemoverProxy::dettach()
{
    //DCHECK(io_task_runner_->belongsToCurrentThread());
    remover_ = nullptr;
}

void FileRemoverProxy::stop()
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(std::bind(&FileRemoverProxy::stop, shared_from_this()));
        return;
    }

    if (remover_)
        remover_->stop();
}

void FileRemoverProxy::setAction(FileRemover::Action action)
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(
            std::bind(&FileRemoverProxy::setAction, shared_from_this(), action));
        return;
    }

    if (remover_)
        remover_->setAction(action);
}

} // namespace client
