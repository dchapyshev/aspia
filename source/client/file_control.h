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

#ifndef CLIENT_FILE_CONTROL_H
#define CLIENT_FILE_CONTROL_H

#include "client/file_remover.h"
#include "client/file_transfer.h"

namespace client {

class FileControl
{
public:
    virtual ~FileControl() = default;

    virtual void driveList(common::FileTask::Target target) = 0;
    virtual void fileList(common::FileTask::Target target, const std::string& path) = 0;
    virtual void createDirectory(common::FileTask::Target target, const std::string& path) = 0;

    virtual void rename(common::FileTask::Target target,
                        const std::string& old_path,
                        const std::string& new_path) = 0;

    virtual void remove(FileRemover* remover) = 0;

    virtual void transfer(std::shared_ptr<FileTransferWindowProxy> transfer_window_proxy,
                          FileTransfer::Type transfer_type,
                          const std::string& source_path,
                          const std::string& target_path,
                          const std::vector<FileTransfer::Item>& items) = 0;
};

} // namespace client

#endif // CLIENT_FILE_CONTROL_H
