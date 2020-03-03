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

#ifndef CLIENT__FILE_REMOVE_WINDOW_PROXY_H
#define CLIENT__FILE_REMOVE_WINDOW_PROXY_H

#include "base/macros_magic.h"
#include "proto/file_transfer.pb.h"

#include <filesystem>
#include <memory>

namespace base {
class TaskRunner;
} // namespace base

namespace client {

class FileRemoveWindow;
class FileRemoverProxy;

class FileRemoveWindowProxy : public std::enable_shared_from_this<FileRemoveWindowProxy>
{
public:
    FileRemoveWindowProxy(
        std::shared_ptr<base::TaskRunner> ui_task_runner, FileRemoveWindow* remove_window);
    ~FileRemoveWindowProxy();

    void dettach();

    void start(std::shared_ptr<FileRemoverProxy> remover_proxy);
    void stop();

    void setCurrentProgress(const std::string& name, int percentage);
    void errorOccurred(const std::string& path,
                       proto::FileError error_code,
                       uint32_t available_actions);

private:
    std::shared_ptr<base::TaskRunner> ui_task_runner_;
    FileRemoveWindow* remove_window_;

    DISALLOW_COPY_AND_ASSIGN(FileRemoveWindowProxy);
};

} // namespace client

#endif // CLIENT__FILE_REMOVE_WINDOW_PROXY_H
