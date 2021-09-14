//
// Aspia Project
// Copyright (C) 2021 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__SCOPED_TASK_RUNNER_H
#define BASE__SCOPED_TASK_RUNNER_H

#include "base/macros_magic.h"
#include "base/task_runner.h"

namespace base {

class ScopedTaskRunner
{
public:
    explicit ScopedTaskRunner(std::shared_ptr<TaskRunner> task_runner);
    ~ScopedTaskRunner();

    void postTask(TaskRunner::Callback callback);

private:
    class Impl;
    std::shared_ptr<Impl> impl_;

    DISALLOW_COPY_AND_ASSIGN(ScopedTaskRunner);
};

} // namespace base

#endif // BASE__SCOPED_TASK_RUNNER_H
