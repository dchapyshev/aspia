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

#include "client/file_transfer_proxy.h"

#include "base/logging.h"
#include "base/task_runner.h"

namespace client {

FileTransferProxy::FileTransferProxy(
    std::shared_ptr<base::TaskRunner> io_task_runner, FileTransfer* file_transfer)
    : io_task_runner_(std::move(io_task_runner)),
      file_transfer_(file_transfer)
{
    DCHECK(io_task_runner_);
    DCHECK(io_task_runner_->belongsToCurrentThread());
    DCHECK(file_transfer_);
}

FileTransferProxy::~FileTransferProxy()
{
    DCHECK(!file_transfer_);
}

void FileTransferProxy::dettach()
{
    DCHECK(io_task_runner_->belongsToCurrentThread());
    file_transfer_ = nullptr;
}

void FileTransferProxy::stop()
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(std::bind(&FileTransferProxy::stop, shared_from_this()));
        return;
    }

    if (file_transfer_)
        file_transfer_->stop();
}

void FileTransferProxy::setAction(
    FileTransfer::Error::Type error_type, FileTransfer::Error::Action action)
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(
            std::bind(&FileTransferProxy::setAction, shared_from_this(), error_type, action));
        return;
    }

    if (file_transfer_)
        file_transfer_->setAction(error_type, action);
}

} // namespace client
