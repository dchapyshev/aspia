//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef COMMON_FILE_TASK_CONSUMER_PROXY_H
#define COMMON_FILE_TASK_CONSUMER_PROXY_H

#include "base/macros_magic.h"
#include "common/file_task_consumer.h"

namespace common {

class FileTaskConsumerProxy : public FileTaskConsumer
{
public:
    explicit FileTaskConsumerProxy(FileTaskConsumer* task_consumer);
    ~FileTaskConsumerProxy() override;

    void dettach();

    // FileTaskConsumer implementation.
    void doTask(std::shared_ptr<FileTask> task) override;

private:
    FileTaskConsumer* task_consumer_;

    DISALLOW_COPY_AND_ASSIGN(FileTaskConsumerProxy);
};

} // namespace common

#endif // COMMON_FILE_TASK_CONSUMER_PROXY_H
