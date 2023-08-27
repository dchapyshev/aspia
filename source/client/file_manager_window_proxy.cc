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

#include "client/file_manager_window_proxy.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "client/file_manager_window.h"

namespace client {

//--------------------------------------------------------------------------------------------------
FileManagerWindowProxy::FileManagerWindowProxy(
    std::shared_ptr<base::TaskRunner> ui_task_runner, FileManagerWindow* file_manager_window)
    : ui_task_runner_(std::move(ui_task_runner)),
      file_manager_window_(file_manager_window)
{
    LOG(LS_INFO) << "Ctor";
    DCHECK(ui_task_runner_);
    DCHECK(ui_task_runner_->belongsToCurrentThread());
    DCHECK(file_manager_window_);
}

//--------------------------------------------------------------------------------------------------
FileManagerWindowProxy::~FileManagerWindowProxy()
{
    LOG(LS_INFO) << "Dtor";
    DCHECK(!file_manager_window_);
}

//--------------------------------------------------------------------------------------------------
void FileManagerWindowProxy::dettach()
{
    LOG(LS_INFO) << "Dettach file manager window";
    DCHECK(ui_task_runner_->belongsToCurrentThread());
    file_manager_window_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void FileManagerWindowProxy::start(std::shared_ptr<FileControlProxy> file_control_proxy)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&FileManagerWindowProxy::start, shared_from_this(), file_control_proxy));
        return;
    }

    if (file_manager_window_)
        file_manager_window_->start(file_control_proxy);
}

//--------------------------------------------------------------------------------------------------
void FileManagerWindowProxy::onErrorOccurred(proto::FileError error_code)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&FileManagerWindowProxy::onErrorOccurred, shared_from_this(), error_code));
        return;
    }

    if (file_manager_window_)
        file_manager_window_->onErrorOccurred(error_code);
}

//--------------------------------------------------------------------------------------------------
void FileManagerWindowProxy::onDriveList(
    common::FileTask::Target target, proto::FileError error_code, const proto::DriveList& drive_list)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(&FileManagerWindowProxy::onDriveList,
                                            shared_from_this(),
                                            target,
                                            error_code,
                                            drive_list));
        return;
    }

    if (file_manager_window_)
        file_manager_window_->onDriveList(target, error_code, drive_list);
}

//--------------------------------------------------------------------------------------------------
void FileManagerWindowProxy::onFileList(
    common::FileTask::Target target, proto::FileError error_code, const proto::FileList& file_list)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(&FileManagerWindowProxy::onFileList,
                                            shared_from_this(),
                                            target,
                                            error_code,
                                            file_list));
        return;
    }

    if (file_manager_window_)
        file_manager_window_->onFileList(target, error_code, file_list);
}

//--------------------------------------------------------------------------------------------------
void FileManagerWindowProxy::onCreateDirectory(
    common::FileTask::Target target, proto::FileError error_code)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(&FileManagerWindowProxy::onCreateDirectory,
                                            shared_from_this(),
                                            target,
                                            error_code));
        return;
    }

    if (file_manager_window_)
        file_manager_window_->onCreateDirectory(target, error_code);
}

//--------------------------------------------------------------------------------------------------
void FileManagerWindowProxy::onRename(common::FileTask::Target target, proto::FileError error_code)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&FileManagerWindowProxy::onRename, shared_from_this(), target, error_code));
        return;
    }

    if (file_manager_window_)
        file_manager_window_->onRename(target, error_code);
}

} // namespace client
