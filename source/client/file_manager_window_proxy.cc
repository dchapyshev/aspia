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

#include "client/file_manager_window_proxy.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "client/file_manager_window.h"

namespace client {

class FileManagerWindowProxy::Impl : public std::enable_shared_from_this<Impl>
{
public:
    Impl(std::shared_ptr<base::TaskRunner> ui_task_runner, FileManagerWindow* file_manager_window);
    ~Impl();

    void dettach();

    void start(std::shared_ptr<FileControlProxy> file_control_proxy);
    void onErrorOccurred(proto::FileError error_code);
    void onDriveList(common::FileTask::Target target,
                     proto::FileError error_code,
                     const proto::DriveList& drive_list);
    void onFileList(common::FileTask::Target target,
                    proto::FileError error_code,
                    const proto::FileList& file_list);
    void onCreateDirectory(common::FileTask::Target target, proto::FileError error_code);
    void onRename(common::FileTask::Target target, proto::FileError error_code);

private:
    std::shared_ptr<base::TaskRunner> ui_task_runner_;
    FileManagerWindow* file_manager_window_;

    DISALLOW_COPY_AND_ASSIGN(Impl);
};

FileManagerWindowProxy::Impl::Impl(
    std::shared_ptr<base::TaskRunner> ui_task_runner, FileManagerWindow* file_manager_window)
    : ui_task_runner_(std::move(ui_task_runner)),
      file_manager_window_(file_manager_window)
{
    DCHECK(ui_task_runner_);
    DCHECK(ui_task_runner_->belongsToCurrentThread());
    DCHECK(file_manager_window_);
}

FileManagerWindowProxy::Impl::~Impl()
{
    DCHECK(!file_manager_window_);
}

void FileManagerWindowProxy::Impl::dettach()
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(&Impl::dettach, shared_from_this()));
        return;
    }

    file_manager_window_ = nullptr;
}

void FileManagerWindowProxy::Impl::start(std::shared_ptr<FileControlProxy> file_manager_proxy)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(std::bind(&Impl::start, shared_from_this(), file_manager_proxy));
        return;
    }

    if (file_manager_window_)
        file_manager_window_->start(file_manager_proxy);
}

void FileManagerWindowProxy::Impl::onErrorOccurred(proto::FileError error_code)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&Impl::onErrorOccurred, shared_from_this(), error_code));
        return;
    }

    if (file_manager_window_)
        file_manager_window_->onErrorOccurred(error_code);
}

void FileManagerWindowProxy::Impl::onDriveList(
    common::FileTask::Target target, proto::FileError error_code, const proto::DriveList& drive_list)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&Impl::onDriveList, shared_from_this(), target, error_code, drive_list));
        return;
    }

    if (file_manager_window_)
        file_manager_window_->onDriveList(target, error_code, drive_list);
}

void FileManagerWindowProxy::Impl::onFileList(
    common::FileTask::Target target, proto::FileError error_code, const proto::FileList& file_list)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&Impl::onFileList, shared_from_this(), target, error_code, file_list));
        return;
    }

    if (file_manager_window_)
        file_manager_window_->onFileList(target, error_code, file_list);
}

void FileManagerWindowProxy::Impl::onCreateDirectory(
    common::FileTask::Target target, proto::FileError error_code)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&Impl::onCreateDirectory, shared_from_this(), target, error_code));
        return;
    }

    if (file_manager_window_)
        file_manager_window_->onCreateDirectory(target, error_code);
}

void FileManagerWindowProxy::Impl::onRename(
    common::FileTask::Target target, proto::FileError error_code)
{
    if (!ui_task_runner_->belongsToCurrentThread())
    {
        ui_task_runner_->postTask(
            std::bind(&Impl::onRename, shared_from_this(), target, error_code));
        return;
    }

    if (file_manager_window_)
        file_manager_window_->onRename(target, error_code);
}

FileManagerWindowProxy::FileManagerWindowProxy(
    std::shared_ptr<base::TaskRunner> ui_task_runner, FileManagerWindow* file_manager_window)
    : impl_(std::make_shared<Impl>(std::move(ui_task_runner), file_manager_window))
{
    // Nothing
}

FileManagerWindowProxy::~FileManagerWindowProxy()
{
    impl_->dettach();
}

// static
std::unique_ptr<FileManagerWindowProxy> FileManagerWindowProxy::create(
    std::shared_ptr<base::TaskRunner> ui_task_runner, FileManagerWindow* file_manager_window)
{
    if (!ui_task_runner || !file_manager_window)
        return nullptr;

    return std::unique_ptr<FileManagerWindowProxy>(
        new FileManagerWindowProxy(std::move(ui_task_runner), file_manager_window));
}

void FileManagerWindowProxy::start(std::shared_ptr<FileControlProxy> file_control_proxy)
{
    impl_->start(file_control_proxy);
}

void FileManagerWindowProxy::onErrorOccurred(proto::FileError error_code)
{
    impl_->onErrorOccurred(error_code);
}

void FileManagerWindowProxy::onDriveList(
    common::FileTask::Target target, proto::FileError error_code, const proto::DriveList& drive_list)
{
    impl_->onDriveList(target, error_code, drive_list);
}

void FileManagerWindowProxy::onFileList(
    common::FileTask::Target target, proto::FileError error_code, const proto::FileList& file_list)
{
    impl_->onFileList(target, error_code, file_list);
}

void FileManagerWindowProxy::onCreateDirectory(
    common::FileTask::Target target, proto::FileError error_code)
{
    impl_->onCreateDirectory(target, error_code);
}

void FileManagerWindowProxy::onRename(common::FileTask::Target target, proto::FileError error_code)
{
    impl_->onRename(target, error_code);
}

} // namespace client
