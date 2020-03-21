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

#ifndef COMMON__FILE_WORKER_H
#define COMMON__FILE_WORKER_H

#include "base/macros_magic.h"

#include <memory>

namespace base {
class TaskRunner;
} // namespace base

namespace common {

class FileTask;

class FileWorker
{
public:
    explicit FileWorker(std::shared_ptr<base::TaskRunner> task_runner);
    ~FileWorker();

    void doTask(std::shared_ptr<FileTask> task);

private:
    class Impl;
    std::shared_ptr<Impl> impl_;

    DISALLOW_COPY_AND_ASSIGN(FileWorker);
};

} // namespace common

#endif // COMMON__FILE_WORKER_H
