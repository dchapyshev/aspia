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

#ifndef CLIENT__FILE_TRANSFER_WINDOW_PROXY_H
#define CLIENT__FILE_TRANSFER_WINDOW_PROXY_H

#include "client/file_transfer.h"

namespace base {
class TaskRunner;
} // namespace base

namespace client {

class FileTransferWindow;

class FileTransferWindowProxy : public std::enable_shared_from_this<FileTransferWindowProxy>
{
public:
    FileTransferWindowProxy(
        std::shared_ptr<base::TaskRunner> ui_task_runner, FileTransferWindow* file_transfer_window);
    ~FileTransferWindowProxy();

    void dettach();

    void start(std::shared_ptr<FileTransferProxy> file_transfer_proxy);
    void stop();
    void setCurrentItem(const std::string& source_path,
                        const std::string& target_path);
    void setCurrentProgress(int total, int current);
    void errorOccurred(const FileTransfer::Error& error);

private:
    std::shared_ptr<base::TaskRunner> ui_task_runner_;
    FileTransferWindow* file_transfer_window_;

    DISALLOW_COPY_AND_ASSIGN(FileTransferWindowProxy);
};

} // namespace client

#endif // CLIENT__FILE_TRANSFER_WINDOW_PROXY_H
