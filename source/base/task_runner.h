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

#ifndef BASE__TASK_RUNNER_H
#define BASE__TASK_RUNNER_H

#include "base/macros_magic.h"

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
    virtual void postTask(const Callback& task) = 0;
    virtual void postDelayedTask(const Callback& callback, const Milliseconds& delay) = 0;
    virtual void postNonNestableTask(const Callback& callback) = 0;
    virtual void postNonNestableDelayedTask(const Callback& callback, const Milliseconds& delay) = 0;
    virtual void postQuit() = 0;

    // Template helpers which use function indirection to erase T from the function signature while
    // still remembering it so we can call the correct destructor/release function.
    //
    // We use this trick so we don't need to include bind.h in a header file like task_runner.h.
    // We also wrap the helpers in a templated class to make it easier for users of DeleteSoon to
    // declare the helper as a friend.
    template <class T>
    class DeleteHelper
    {
    public:
        static void doDelete(const void* object)
        {
            delete reinterpret_cast<const T*>(object);
        }

    private:
        DISALLOW_COPY_AND_ASSIGN(DeleteHelper);
    };

    // An internal TaskRunner-like class helper for DeleteHelper. We don't want to expose the do*()
    // functions directly since the void* argument makes it possible to pass/ an object of the
    // wrong type to delete. Instead, we force callers to go through these internal helpers for
    // type safety. TaskRunner-like classes which expose deleteSoon or method should friend the
    // appropriate helper and implement a corresponding *Internal method with the following
    // signature:
    //
    // bool(void(*function)(const void*), void* object)
    //
    // An implementation of this function should simply create a std::function from (function,
    // object) and return the result of posting the task.
    template <class T, class ReturnType>
    class DeleteHelperInternal
    {
    public:
        template <class TaskRunnerType>
        static ReturnType deleteViaTaskRunner(TaskRunnerType* task_runner, const T* object)
        {
            return task_runner->deleteSoonInternal(&DeleteHelper<T>::doDelete, object);
        }

    private:
        DISALLOW_COPY_AND_ASSIGN(DeleteHelperInternal);
    };

    template <class T>
    void deleteSoon(const T* object)
    {
        DeleteHelperInternal<T, void>::deleteViaTaskRunner(this, object);
    }

private:
    void deleteSoonInternal(void(*deleter)(const void*), const void* object)
    {
        postNonNestableTask(std::bind(deleter, object));
    }
};

} // namespace base

#endif // BASE__TASK_RUNNER_H
