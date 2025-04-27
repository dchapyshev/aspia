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

#ifndef CLIENT_FILE_CONTROL_PROXY_H
#define CLIENT_FILE_CONTROL_PROXY_H

#include "client/file_control.h"

namespace base {
class TaskRunner;
} // namespace base

namespace client {

class FileControlProxy final : public std::enable_shared_from_this<FileControlProxy>
{
public:
    FileControlProxy(std::shared_ptr<base::TaskRunner> io_task_runner, FileControl* file_control);
    ~FileControlProxy();

    void dettach();

    void driveList(common::FileTask::Target target);
    void fileList(common::FileTask::Target target, const std::string& path);
    void createDirectory(common::FileTask::Target target, const std::string& path);

    void rename(common::FileTask::Target target,
                const std::string& old_path,
                const std::string& new_path);

    void remove(FileRemover* remover);
    void transfer(FileTransfer* transfer);

private:
    std::shared_ptr<base::TaskRunner> io_task_runner_;
    FileControl* file_control_;

    DISALLOW_COPY_AND_ASSIGN(FileControlProxy);
};

} // namespace client

#endif // CLIENT_FILE_CONTROL_PROXY_H
