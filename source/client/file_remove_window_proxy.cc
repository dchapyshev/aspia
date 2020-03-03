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

#include "client/file_remove_window_proxy.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "client/file_remove_window.h"

namespace client {

FileRemoveWindowProxy::FileRemoveWindowProxy(
    std::shared_ptr<base::TaskRunner> ui_task_runner, FileRemoveWindow* remove_window)
    : ui_task_runner_(std::move(ui_task_runner)),
      remove_window_(remove_window)
{
    DCHECK(ui_task_runner_);
    DCHECK(ui_task_runner_->belongsToCurrentThread());
    DCHECK(remove_window_);
}

FileRemoveWindowProxy::~FileRemoveWindowProxy()
{
    DCHECK(!remove_window_);
}

void FileRemoveWindowProxy::dettach()
{
    DCHECK(ui_task_runner_->belongsToCurrentThread());
    remove_window_ = nullptr;
}

void FileRemoveWindowProxy::start(std::shared_ptr<FileRemoverProxy> remover_proxy)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&FileRemoveWindowProxy::start, shared_from_this(), remover_proxy));
        return;
    }

    if (remove_window_)
        remove_window_->start(remover_proxy);
}

void FileRemoveWindowProxy::stop()
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(&FileRemoveWindowProxy::stop, shared_from_this()));
        return;
    }

    if (remove_window_)
        remove_window_->stop();
}

void FileRemoveWindowProxy::setCurrentProgress(const std::string& name, int percentage)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(
            &FileRemoveWindowProxy::setCurrentProgress, shared_from_this(), name, percentage));
        return;
    }

    if (remove_window_)
        remove_window_->setCurrentProgress(name, percentage);
}

void FileRemoveWindowProxy::errorOccurred(const std::string& path,
                                          proto::FileError error_code,
                                          uint32_t available_actions)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&FileRemoveWindowProxy::errorOccurred,
                      shared_from_this(),
                      path,
                      error_code,
                      available_actions));
        return;
    }

    if (remove_window_)
        remove_window_->errorOccurred(path, error_code, available_actions);
}

} // namespace client
