//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/message_loop/pending_task.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__MESSAGE_LOOP__PENDING_TASK_H
#define _ASPIA_BASE__MESSAGE_LOOP__PENDING_TASK_H

#include <chrono>
#include <functional>
#include <queue>

namespace aspia {

// Contains data about a pending task. Stored in TaskQueue and DelayedTaskQueue for use by classes
// that queue and execute tasks.
class PendingTask
{
public:
    using Callback = std::function<void()>;
    using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;
    using TimeDelta = std::chrono::milliseconds;

    PendingTask(Callback callback);
    PendingTask(const Callback& callback, const TimePoint& delayed_run_time);
    ~PendingTask() = default;

    // Used to support sorting.
    bool operator<(const PendingTask& other) const;

    // Secondary sort key for run time.
    int sequence_num = 0;

    TimePoint delayed_run_time;

    // The task to run.
    Callback callback;
};

// Wrapper around std::queue specialized for PendingTask which adds a Swap helper method.
class TaskQueue : public std::queue<PendingTask>
{
public:
    void Swap(TaskQueue& queue)
    {
        c.swap(queue.c); // Calls std::deque::swap.
    }
};

// PendingTasks are sorted by their |delayed_run_time| property.
using DelayedTaskQueue = std::priority_queue<PendingTask>;

} // namespace aspia

#endif // _ASPIA_BASE__MESSAGE_LOOP__PENDING_TASK_H
