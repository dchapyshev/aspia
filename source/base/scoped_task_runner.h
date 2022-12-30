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

#ifndef BASE_SCOPED_TASK_RUNNER_H
#define BASE_SCOPED_TASK_RUNNER_H

#include "base/macros_magic.h"
#include "base/task_runner.h"
#include "base/memory/local_memory.h"

namespace base {

class ScopedTaskRunner
{
public:
    explicit ScopedTaskRunner(std::shared_ptr<TaskRunner> task_runner);
    ~ScopedTaskRunner();

    void postTask(TaskRunner::Callback callback);
    void postDelayedTask(const std::chrono::milliseconds& timeout, TaskRunner::Callback callback);

private:
    class Impl;
    base::local_shared_ptr<Impl> impl_;

    DISALLOW_COPY_AND_ASSIGN(ScopedTaskRunner);
};

} // namespace base

#endif // BASE_SCOPED_TASK_RUNNER_H
