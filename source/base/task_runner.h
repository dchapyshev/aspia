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

#ifndef BASE_TASK_RUNNER_H
#define BASE_TASK_RUNNER_H

#include "base/macros_magic.h"

#include <QThread>

#include <chrono>
#include <functional>
#include <memory>

namespace base {

class TaskRunner : public std::enable_shared_from_this<TaskRunner>
{
public:
    TaskRunner();
    ~TaskRunner();

    using Callback = std::function<void()>;
    using Milliseconds = std::chrono::milliseconds;

    bool belongsToCurrentThread() const;
    void postTask(Callback callback);
    void postDelayedTask(Callback callback, const Milliseconds& delay);
    void postNonNestableTask(Callback callback);
    void postNonNestableDelayedTask(Callback callback, const Milliseconds& delay);
    void postQuit();

    template <class T>
    static void doDelete(const void* object)
    {
        delete static_cast<const T*>(object);
    }

    template <class T>
    void deleteSoon(const T* object)
    {
        deleteSoonInternal(&TaskRunner::doDelete<T>, object);
    }

    template <class T>
    void deleteSoon(std::unique_ptr<T> object)
    {
        deleteSoon(object.release());
    }

private:
    void deleteSoonInternal(void(*deleter)(const void*), const void* object);

    class Impl;
    std::unique_ptr<Impl> impl_;

    DISALLOW_COPY_AND_ASSIGN(TaskRunner);
};

} // namespace base

#endif // BASE_TASK_RUNNER_H
