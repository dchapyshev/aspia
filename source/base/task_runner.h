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

#ifndef BASE__TASK_RUNNER_H
#define BASE__TASK_RUNNER_H

#include <chrono>
#include <functional>
#include <memory>

namespace base {

class TaskRunner : public std::enable_shared_from_this<TaskRunner>
{
public:
    virtual ~TaskRunner() = default;

    using Callback = std::function<void()>;
    using Milliseconds = std::chrono::milliseconds;

    virtual bool belongsToCurrentThread() const = 0;
    virtual bool postTask(Callback task) = 0;
    virtual bool postDelayedTask(Callback callback, const Milliseconds& delay) = 0;
};

} // namespace base

#endif // BASE__TASK_RUNNER_H
