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

#include "client/file_transfer_window_proxy.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "client/file_transfer_window.h"

namespace client {

FileTransferWindowProxy::FileTransferWindowProxy(
    std::shared_ptr<base::TaskRunner> ui_task_runner, FileTransferWindow* file_transfer_window)
    : ui_task_runner_(std::move(ui_task_runner)),
      file_transfer_window_(file_transfer_window)
{
    DCHECK(ui_task_runner_);
    DCHECK(ui_task_runner_->belongsToCurrentThread());
    DCHECK(file_transfer_window_);
}

FileTransferWindowProxy::~FileTransferWindowProxy()
{
    DCHECK(!file_transfer_window_);
}

void FileTransferWindowProxy::dettach()
{
    DCHECK(ui_task_runner_->belongsToCurrentThread());
    file_transfer_window_ = nullptr;
}

void FileTransferWindowProxy::start(std::shared_ptr<FileTransferProxy> file_transfer_proxy)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&FileTransferWindowProxy::start, shared_from_this(), file_transfer_proxy));
        return;
    }

    if (file_transfer_window_)
        file_transfer_window_->start(file_transfer_proxy);
}

void FileTransferWindowProxy::stop()
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(&FileTransferWindowProxy::stop, shared_from_this()));
        return;
    }

    if (file_transfer_window_)
        file_transfer_window_->stop();
}

void FileTransferWindowProxy::setCurrentItem(
    const std::string& source_path, const std::string& target_path)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(
            &FileTransferWindowProxy::setCurrentItem, shared_from_this(), source_path, target_path));
        return;
    }

    if (file_transfer_window_)
        file_transfer_window_->setCurrentItem(source_path, target_path);
}

void FileTransferWindowProxy::setCurrentProgress(int total, int current)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(
            &FileTransferWindowProxy::setCurrentProgress, shared_from_this(), total, current));
        return;
    }

    if (file_transfer_window_)
        file_transfer_window_->setCurrentProgress(total, current);
}

void FileTransferWindowProxy::errorOccurred(const FileTransfer::Error& error)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&FileTransferWindowProxy::errorOccurred, shared_from_this(), error));
        return;
    }

    if (file_transfer_window_)
        file_transfer_window_->errorOccurred(error);
}

} // namespace client
