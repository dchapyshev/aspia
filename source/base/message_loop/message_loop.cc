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

#include "base/message_loop/message_loop.h"

#include "base/message_loop/message_loop_task_runner.h"
#include "base/message_loop/message_pump_asio.h"
#include "base/message_loop/message_pump_default.h"
#include "base/logging.h"

#if defined(OS_WIN)
#include "base/message_loop/message_pump_win.h"
#endif // defined(OS_WIN)

#include <memory>

namespace base {

static thread_local MessageLoop* message_loop_for_current_thread = nullptr;

// static
MessageLoop* MessageLoop::current()
{
    return message_loop_for_current_thread;
}

MessageLoop::MessageLoop(Type type)
    : type_(type)
{
    DCHECK(!current()) << "should only have one message loop per thread";

    message_loop_for_current_thread = this;

    proxy_.reset(new MessageLoopTaskRunner(this));

    switch (type)
    {
        case Type::ASIO:
            pump_ = std::make_unique<MessagePumpForAsio>();
            break;

#if defined(OS_WIN)
        case Type::WIN:
            pump_ = std::make_unique<MessagePumpForWin>();
            break;
#endif // defined(OS_WIN)

        default:
            pump_ = std::make_unique<MessagePumpDefault>();
            break;
    }
}

MessageLoop::~MessageLoop()
{
    DCHECK_EQ(this, current());

    // Clean up any unprocessed tasks, but take care: deleting a task could result in the addition
    // of more tasks (e.g., via DeleteSoon). We set a limit on the number of times we will allow a
    // deleted task to generate more tasks. Normally, we should only pass through this loop once or
    // twice. If we end up hitting the loop limit, then it is probably due to one task that is
    // being stubborn. Inspect the queues to see who is left.
    bool did_work;
    for (int i = 0; i < 100; ++i)
    {
        reloadWorkQueue();
        deletePendingTasks();

        // If we end up with empty queues, then break out of the loop.
        did_work = deletePendingTasks();
        if (!did_work)
            break;
    }

    DCHECK(!did_work);

    proxy_->willDestroyCurrentMessageLoop();
    proxy_ = nullptr;

    message_loop_for_current_thread = nullptr;
}

void MessageLoop::run(Dispatcher* dispatcher)
{
    DCHECK_EQ(this, current());

    if (dispatcher && type() == Type::WIN)
    {
        pumpWin()->runWithDispatcher(this, dispatcher);
        return;
    }

    pump_->run(this);
}

void MessageLoop::quit()
{
    DCHECK_EQ(this, current());
    pump_->quit();
}

PendingTask::Callback MessageLoop::quitClosure()
{
    return std::bind(&MessageLoop::quit, this);
}

void MessageLoop::postTask(PendingTask::Callback callback)
{
    DCHECK(callback != nullptr);
    addToIncomingQueue(std::move(callback), Milliseconds::zero(), true);
}

void MessageLoop::postDelayedTask(PendingTask::Callback callback, Milliseconds delay)
{
    DCHECK(callback != nullptr);
    addToIncomingQueue(std::move(callback), delay, true);
}

void MessageLoop::postNonNestableTask(PendingTask::Callback callback)
{
    DCHECK(callback != nullptr);
    addToIncomingQueue(std::move(callback), Milliseconds::zero(), false);
}

void MessageLoop::postNonNestableDelayedTask(PendingTask::Callback callback, Milliseconds delay)
{
    DCHECK(callback != nullptr);
    addToIncomingQueue(std::move(callback), delay, false);
}

#if defined(OS_WIN)
MessagePumpForWin* MessageLoop::pumpWin() const
{
    return static_cast<MessagePumpForWin*>(pump_.get());
}
#endif // defined(OS_WIN)

MessagePumpForAsio* MessageLoop::pumpAsio() const
{
    return static_cast<MessagePumpForAsio*>(pump_.get());
}

std::shared_ptr<TaskRunner> MessageLoop::taskRunner() const
{
    return proxy_;
}

void MessageLoop::runTask(const PendingTask& pending_task)
{
    DCHECK(nestable_tasks_allowed_);

    // Execute the task and assume the worst: It is probably not reentrant.
    nestable_tasks_allowed_ = false;

    pending_task.callback();

    nestable_tasks_allowed_ = true;
}

bool MessageLoop::deferOrRunPendingTask(const PendingTask& pending_task)
{
    if (pending_task.nestable)
    {
        runTask(pending_task);

        // Show that we ran a task (Note: a new one might arrive as a consequence!).
        return true;
    }

    // We couldn't run the task now because we're in a nested message loop
    // and the task isn't nestable.
    deferred_non_nestable_work_queue_.emplace(pending_task);
    return false;
}

void MessageLoop::addToDelayedWorkQueue(PendingTask* pending_task)
{
    // Move to the delayed work queue.  Initialize the sequence number before inserting into the
    // delayed_work_queue_. The sequence number is used to faciliate FIFO sorting when two tasks
    // have the same delayed_run_time value.
    delayed_work_queue_.emplace(std::move(pending_task->callback),
                                pending_task->delayed_run_time,
                                pending_task->nestable,
                                next_sequence_num_++);
}

void MessageLoop::addToIncomingQueue(
    PendingTask::Callback&& callback, Milliseconds delay, bool nestable)
{
    bool empty;

    {
        std::scoped_lock lock(incoming_queue_lock_);

        empty = incoming_queue_.empty();

        incoming_queue_.emplace(std::move(callback),
                                calculateDelayedRuntime(delay),
                                nestable);
    }

    if (!empty)
        return;

    std::shared_ptr<MessagePump> pump(pump_);
    pump->scheduleWork();
}

void MessageLoop::reloadWorkQueue()
{
    if (!work_queue_.empty())
        return;

    std::scoped_lock lock(incoming_queue_lock_);

    if (incoming_queue_.empty())
        return;

    incoming_queue_.Swap(&work_queue_);
    DCHECK(incoming_queue_.empty());
}

bool MessageLoop::deletePendingTasks()
{
    bool did_work = !work_queue_.empty();

    while (!work_queue_.empty())
    {
        PendingTask pending_task = work_queue_.front();
        work_queue_.pop();

        if (pending_task.delayed_run_time != TimePoint())
        {
            // We want to delete delayed tasks in the same order in which they would normally be
            // deleted in case of any funny dependencies between delayed tasks.
            addToDelayedWorkQueue(&pending_task);
        }
    }

    did_work |= !deferred_non_nestable_work_queue_.empty();

    while (!deferred_non_nestable_work_queue_.empty())
        deferred_non_nestable_work_queue_.pop();

    did_work |= !delayed_work_queue_.empty();

    while (!delayed_work_queue_.empty())
        delayed_work_queue_.pop();

    return did_work;
}

// static
MessageLoop::TimePoint MessageLoop::calculateDelayedRuntime(Milliseconds delay)
{
    TimePoint delayed_run_time;

    if (delay > Milliseconds::zero())
        delayed_run_time = Clock::now() + delay;

    return delayed_run_time;
}

bool MessageLoop::doWork()
{
    if (!nestable_tasks_allowed_)
    {
        // Task can't be executed right now.
        return false;
    }

    for (;;)
    {
        reloadWorkQueue();

        if (work_queue_.empty())
            break;

        // Execute oldest task.
        do
        {
            PendingTask pending_task = work_queue_.front();
            work_queue_.pop();

            if (pending_task.delayed_run_time != TimePoint())
            {
                const bool reschedule = delayed_work_queue_.empty();

                addToDelayedWorkQueue(&pending_task);

                // If we changed the topmost task, then it is time to reschedule.
                if (reschedule)
                    pump_->scheduleDelayedWork(pending_task.delayed_run_time);
            }
            else
            {
                if (deferOrRunPendingTask(pending_task))
                    return true;
            }
        }
        while (!work_queue_.empty());
    }

    // Nothing happened.
    return false;
}

bool MessageLoop::doDelayedWork(TimePoint* next_delayed_work_time)
{
    if (!nestable_tasks_allowed_ || delayed_work_queue_.empty())
    {
        recent_time_ = *next_delayed_work_time = TimePoint();
        return false;
    }

    // When we "fall behind," there will be a lot of tasks in the delayed work queue that are ready
    // to run.  To increase efficiency when we fall behind, we will only call Clock::now()
    // intermittently, and then process all tasks that are ready to run before calling it again.
    // As a result, the more we fall behind (and have a lot of ready-to-run delayed tasks), the more
    // efficient we'll be at handling the tasks.

    TimePoint next_run_time = delayed_work_queue_.top().delayed_run_time;

    if (next_run_time > recent_time_)
    {
        recent_time_ = Clock::now();
        if (next_run_time > recent_time_)
        {
            *next_delayed_work_time = next_run_time;
            return false;
        }
    }

    PendingTask pending_task = delayed_work_queue_.top();
    delayed_work_queue_.pop();

    if (!delayed_work_queue_.empty())
        *next_delayed_work_time = delayed_work_queue_.top().delayed_run_time;

    return deferOrRunPendingTask(pending_task);
}

bool MessageLoop::doIdleWork()
{
    if (deferred_non_nestable_work_queue_.empty())
        return false;

    PendingTask pending_task = deferred_non_nestable_work_queue_.front();
    deferred_non_nestable_work_queue_.pop();

    runTask(pending_task);
    return true;
}

} // namespace base
