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

#include "client/file_control_proxy.h"

#include "base/logging.h"
#include "base/task_runner.h"

namespace client {

//--------------------------------------------------------------------------------------------------
FileControlProxy::FileControlProxy(
    std::shared_ptr<base::TaskRunner> io_task_runner, FileControl* file_control)
    : io_task_runner_(std::move(io_task_runner)),
      file_control_(file_control)
{
    LOG(LS_INFO) << "Ctor";
    DCHECK(io_task_runner_);
    DCHECK(file_control_);
}

//--------------------------------------------------------------------------------------------------
FileControlProxy::~FileControlProxy()
{
    LOG(LS_INFO) << "Dtor";
    DCHECK(!file_control_);
}

//--------------------------------------------------------------------------------------------------
void FileControlProxy::dettach()
{
    LOG(LS_INFO) << "Dettach file control";
    DCHECK(io_task_runner_->belongsToCurrentThread());
    file_control_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void FileControlProxy::driveList(common::FileTask::Target target)
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(
            std::bind(&FileControlProxy::driveList, shared_from_this(), target));
        return;
    }

    if (file_control_)
        file_control_->driveList(target);
}

//--------------------------------------------------------------------------------------------------
void FileControlProxy::fileList(common::FileTask::Target target, const std::string& path)
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(
            std::bind(&FileControlProxy::fileList, shared_from_this(), target, path));
        return;
    }

    if (file_control_)
        file_control_->fileList(target, path);
}

//--------------------------------------------------------------------------------------------------
void FileControlProxy::createDirectory(common::FileTask::Target target, const std::string& path)
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(
            std::bind(&FileControlProxy::createDirectory, shared_from_this(), target, path));
        return;
    }

    if (file_control_)
        file_control_->createDirectory(target, path);
}

//--------------------------------------------------------------------------------------------------
void FileControlProxy::rename(
    common::FileTask::Target target, const std::string& old_path, const std::string& new_path)
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(
            std::bind(&FileControlProxy::rename, shared_from_this(), target, old_path, new_path));
        return;
    }

    if (file_control_)
        file_control_->rename(target, old_path, new_path);
}

//--------------------------------------------------------------------------------------------------
void FileControlProxy::remove(FileRemover* remover)
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(std::bind(
            &FileControlProxy::remove, shared_from_this(), remover));
        return;
    }

    if (file_control_)
        file_control_->remove(remover);
}

//--------------------------------------------------------------------------------------------------
void FileControlProxy::transfer(FileTransfer* transfer)
{
    if (!io_task_runner_->belongsToCurrentThread())
    {
        io_task_runner_->postTask(
            std::bind(&FileControlProxy::transfer, shared_from_this(), transfer));
        return;
    }

    if (file_control_)
        file_control_->transfer(transfer);
}

} // namespace client
