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

#ifndef BASE__MESSAGE_LOOP__PENDING_TASK_H
#define BASE__MESSAGE_LOOP__PENDING_TASK_H

#include "base/memory/scalable_queue.h"

#include <chrono>
#include <functional>

namespace base {

// Contains data about a pending task. Stored in TaskQueue and DelayedTaskQueue for use by classes
// that queue and execute tasks.
class PendingTask
{
public:
    using Callback = std::function<void()>;
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    PendingTask(Callback&& callback,
                TimePoint delayed_run_time,
                bool nestable,
                int sequence_num = 0);
    ~PendingTask() = default;

    // Used to support sorting.
    bool operator<(const PendingTask& other) const;

    // The task to run.
    Callback callback;

    // Secondary sort key for run time.
    int sequence_num;

    TimePoint delayed_run_time;

    // OK to dispatch from a nested loop.
    bool nestable;
};

// Wrapper around std::queue specialized for PendingTask which adds a Swap helper method.
class TaskQueue : public ScalableQueue<PendingTask>
{
public:
    void Swap(TaskQueue* queue)
    {
        c.swap(queue->c); // Calls std::deque::swap.
    }
};

// PendingTasks are sorted by their |delayed_run_time| property.
using DelayedTaskQueue = ScalablePriorityQueue<PendingTask>;

} // namespace base

#endif // BASE__MESSAGE_LOOP__PENDING_TASK_H
