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

#ifndef COMMON__FILE_TASK_PRODUCER_PROXY_H
#define COMMON__FILE_TASK_PRODUCER_PROXY_H

#include "base/macros_magic.h"
#include "common/file_task_producer.h"

namespace common {

class FileTaskProducerProxy : public FileTaskProducer
{
public:
    explicit FileTaskProducerProxy(FileTaskProducer* task_producer);
    ~FileTaskProducerProxy();

    void dettach();

    // FileTaskProducer implementation.
    void onTaskDone(std::shared_ptr<FileTask> task) override;

private:
    FileTaskProducer* task_producer_;

    DISALLOW_COPY_AND_ASSIGN(FileTaskProducerProxy);
};

} // namespace common

#endif // COMMON__FILE_REQUEST_PRODUCER_PROXY_H
