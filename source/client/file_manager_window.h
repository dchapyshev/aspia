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

#ifndef CLIENT_FILE_MANAGER_WINDOW_H
#define CLIENT_FILE_MANAGER_WINDOW_H

#include "common/file_task.h"
#include "proto/file_transfer.h"

namespace client {

class FileControlProxy;

class FileManagerWindow
{
public:
    virtual ~FileManagerWindow() = default;

    virtual void start(std::shared_ptr<FileControlProxy> file_control_proxy) = 0;

    virtual void onErrorOccurred(proto::FileError error_code) = 0;

    // Called when a response to a drive list request is received.
    virtual void onDriveList(common::FileTask::Target target,
                             proto::FileError error_code,
                             const proto::DriveList& drive_list) = 0;

    // Called when a response to a file list request is received.
    virtual void onFileList(common::FileTask::Target target,
                            proto::FileError error_code,
                            const proto::FileList& file_list) = 0;

    // Called upon receipt of a response to a directory creation request.
    virtual void onCreateDirectory(common::FileTask::Target target, proto::FileError error_code) = 0;

    // Called upon receipt of a response to a request to rename a file or directory.
    virtual void onRename(common::FileTask::Target target, proto::FileError error_code) = 0;
};

} // namespace client

#endif // CLIENT_FILE_MANAGER_WINDOW_H
