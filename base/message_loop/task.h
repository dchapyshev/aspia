//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/message_loop/task.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__MESSAGE_LOOP__TASK_H
#define _ASPIA_BASE__MESSAGE_LOOP__TASK_H

#include <functional>
#include <queue>

namespace aspia {

// Contains data about a pending task. Stored in TaskQueue and DelayedTaskQueue
// for use by classes that queue and execute tasks.
class Task
{
public:
    using Callback = std::function<void()>;

    Task(Callback callback) :
        callback(std::move(callback))
    {
        // Nothing
    }

    ~Task() {}

    // The task to run.
    Callback callback;
};

// Wrapper around std::queue specialized for PendingTask which adds a Swap
// helper method.
class TaskQueue : public std::queue<Task>
{
public:
    void Swap(TaskQueue* queue)
    {
        c.swap(queue->c); // Calls std::deque::swap.
    }
};

// PendingTasks are sorted by their |delayed_run_time| property.
using DelayedTaskQueue = std::priority_queue<Task>;

} // namespace aspia

#endif // _ASPIA_BASE__MESSAGE_LOOP__TASK_H
