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

#ifndef CLIENT_FILE_MANAGER_WINDOW_PROXY_H
#define CLIENT_FILE_MANAGER_WINDOW_PROXY_H

#include "common/file_task.h"
#include "proto/file_transfer.h"

#include <memory>

namespace base {
class TaskRunner;
} // namespace base

namespace client {

class FileControlProxy;
class FileManagerWindow;

class FileManagerWindowProxy final : public std::enable_shared_from_this<FileManagerWindowProxy>
{
public:
    FileManagerWindowProxy(std::shared_ptr<base::TaskRunner> ui_task_runner,
                           FileManagerWindow* file_manager_window);
    ~FileManagerWindowProxy();

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

    DISALLOW_COPY_AND_ASSIGN(FileManagerWindowProxy);
};

} // namespace client

#endif // CLIENT_FILE_MANAGER_WINDOW_PROXY_H
