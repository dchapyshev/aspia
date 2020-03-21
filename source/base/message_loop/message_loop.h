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

#ifndef BASE__MESSAGE_LOOP__MESSAGE_LOOP_H
#define BASE__MESSAGE_LOOP__MESSAGE_LOOP_H

#include "base/macros_magic.h"
#include "base/task_runner.h"
#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_pump_dispatcher.h"
#include "base/message_loop/pending_task.h"
#include "build/build_config.h"

#include <memory>
#include <mutex>

namespace base {

class MessageLoopTaskRunner;
class MessagePumpForAsio;
class MessagePumpForWin;
class Thread;

class MessageLoop : public MessagePump::Delegate
{
public:
    enum class Type
    {
        DEFAULT,
        ASIO,
#if defined(OS_WIN)
        WIN
#endif // defined(OS_WIN)
    };

    using Dispatcher = MessagePumpDispatcher;

    explicit MessageLoop(Type type = Type::DEFAULT);
    virtual ~MessageLoop();

    Type type() const { return type_; }

    void run(Dispatcher* dispatcher = nullptr);

    static MessageLoop* current();

#if defined(OS_WIN)
    MessagePumpForWin* pumpWin() const;
#endif // defined(OS_WIN)

    MessagePumpForAsio* pumpAsio() const;

    std::shared_ptr<TaskRunner> taskRunner() const;

protected:
    friend class MessageLoopTaskRunner;
    friend class Thread;

    using Clock = MessagePump::Clock;
    using TimePoint = MessagePump::TimePoint;
    using Milliseconds = MessagePump::Milliseconds;

    void postTask(PendingTask::Callback callback);
    void postDelayedTask(PendingTask::Callback callback, Milliseconds delay);
    void postNonNestableTask(PendingTask::Callback callback);
    void postNonNestableDelayedTask(PendingTask::Callback callback, Milliseconds delay);

    PendingTask::Callback quitClosure();

    // Runs the specified PendingTask.
    void runTask(const PendingTask& pending_task);

    // Calls RunTask or queues the pending_task on the deferred task list if it cannot be run right
    // now. Returns true if the task was run.
    bool deferOrRunPendingTask(const PendingTask& pending_task);

    // Adds the pending task to delayed_work_queue_.
    void addToDelayedWorkQueue(PendingTask* pending_task);

    // Adds the pending task to our incoming_queue_.
    //
    // Caller retains ownership of |pending_task|, but this function will reset the value of
    // pending_task->task. This is needed to ensure that the posting call stack does not retain
    // pending_task->task beyond this function call.
    void addToIncomingQueue(PendingTask::Callback&& callback, Milliseconds delay, bool nestable);

    // Load tasks from the incoming_queue_ into work_queue_ if the latter is empty. The former
    // requires a lock to access, while the latter is directly accessible on this thread.
    void reloadWorkQueue();

    bool deletePendingTasks();

    // Calculates the time at which a PendingTask should run.
    static TimePoint calculateDelayedRuntime(Milliseconds delay);

    // MessagePump::Delegate methods:
    bool doWork() override;
    bool doDelayedWork(TimePoint* next_delayed_work_time) override;
    bool doIdleWork() override;

    const Type type_;

    // A recent snapshot of Clock::now(), used to check delayed_work_queue_.
    TimePoint recent_time_;

    // Contains delayed tasks, sorted by their 'delayed_run_time' property.
    DelayedTaskQueue delayed_work_queue_;

    // A list of tasks that need to be processed by this instance.  Note that this queue is only
    // accessed (push/pop) by our current thread.
    TaskQueue work_queue_;

    // A queue of non-nestable tasks that we had to defer because when it came time to execute them
    // we were in a nested message loop. They will execute once we're out of nested message loops.
    TaskQueue deferred_non_nestable_work_queue_;

    // A recursion block that prevents accidentally running additional tasks when insider a
    // (accidentally induced?) nested message pump.
    bool nestable_tasks_allowed_ = true;

    std::shared_ptr<MessagePump> pump_;

    TaskQueue incoming_queue_;
    std::mutex incoming_queue_lock_;

    // The next sequence number to use for delayed tasks.
    int next_sequence_num_ = 0;

    std::shared_ptr<MessageLoopTaskRunner> proxy_;

private:
    void quit();

    DISALLOW_COPY_AND_ASSIGN(MessageLoop);
};

} // namespace base

#endif // BASE__MESSAGE_LOOP__MESSAGE_LOOP_H
