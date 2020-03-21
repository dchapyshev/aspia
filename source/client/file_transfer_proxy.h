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

#ifndef CLIENT__FILE_TRANSFER_PROXY_H
#define CLIENT__FILE_TRANSFER_PROXY_H

#include "base/macros_magic.h"
#include "client/file_transfer.h"

#include <memory>

namespace base {
class TaskRunner;
} // namespace base

namespace client {

class FileTransferProxy : public std::enable_shared_from_this<FileTransferProxy>
{
public:
    FileTransferProxy(std::shared_ptr<base::TaskRunner> io_task_runner, FileTransfer* file_transfer);
    ~FileTransferProxy();

    void dettach();

    void stop();
    void setAction(FileTransfer::Error::Type error_type, FileTransfer::Error::Action action);

private:
    std::shared_ptr<base::TaskRunner> io_task_runner_;
    FileTransfer* file_transfer_;

    DISALLOW_COPY_AND_ASSIGN(FileTransferProxy);
};

} // namespace client

#endif // CLIENT__FILE_TRANSFER_PROXY_H
