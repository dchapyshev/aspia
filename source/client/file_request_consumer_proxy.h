//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT__FILE_REQUEST_CONSUMER_PROXY_H
#define CLIENT__FILE_REQUEST_CONSUMER_PROXY_H

#include "base/macros_magic.h"
#include "proto/file_transfer.pb.h"

namespace base {
class TaskRunner;
} // namespace base

namespace client {

class FileRequestConsumer;

class FileRequestConsumerProxy
{
public:
    FileRequestConsumerProxy(const std::weak_ptr<FileRequestConsumer>& request_consumer,
                             std::unique_ptr<base::TaskRunner> task_runner);
    ~FileRequestConsumerProxy();

    void localRequest(const proto::FileRequest& request);
    void remoteRequest(const proto::FileRequest& request);

private:
    std::weak_ptr<FileRequestConsumer> request_consumer_;
    std::unique_ptr<base::TaskRunner> task_runner_;

    DISALLOW_COPY_AND_ASSIGN(FileRequestConsumerProxy);
};

} // namespace client

#endif // CLIENT__FILE_REQUEST_CONSUMER_PROXY_H
