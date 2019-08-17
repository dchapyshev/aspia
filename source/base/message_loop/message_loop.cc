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

#include "base/message_loop/message_loop.h"

#include "base/message_loop/message_loop_proxy.h"
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

    proxy_.reset(new MessageLoopProxy(this));

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

    reloadWorkQueue();
    deletePendingTasks();

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

    PendingTask pending_task(std::move(callback), calculateDelayedRuntime(Milliseconds::zero()));
    addToIncomingQueue(pending_task);
}

void MessageLoop::postDelayedTask(PendingTask::Callback callback, const Milliseconds& delay)
{
    DCHECK(callback != nullptr);

    PendingTask pending_task(std::move(callback), calculateDelayedRuntime(delay));
    addToIncomingQueue(pending_task);
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

std::shared_ptr<MessageLoopProxy> MessageLoop::messageLoopProxy() const
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

void MessageLoop::addToDelayedWorkQueue(const PendingTask& pending_task)
{
    // Move to the delayed work queue.  Initialize the sequence number before inserting into the
    // delayed_work_queue_. The sequence number is used to faciliate FIFO sorting when two tasks
    // have the same delayed_run_time value.
    PendingTask new_pending_task(pending_task);
    new_pending_task.sequence_num = next_sequence_num_++;
    delayed_work_queue_.emplace(new_pending_task);
}

void MessageLoop::addToIncomingQueue(PendingTask& pending_task)
{
    std::shared_ptr<MessagePump> pump;

    {
        std::scoped_lock lock(incoming_queue_lock_);

        const bool empty = incoming_queue_.empty();

        incoming_queue_.emplace(pending_task);

        pending_task.callback = nullptr;

        if (!empty)
            return;

        pump = pump_;
    }

    pump->scheduleWork();
}

void MessageLoop::reloadWorkQueue()
{
    if (!work_queue_.empty())
        return;

    {
        std::scoped_lock lock(incoming_queue_lock_);

        if (incoming_queue_.empty())
            return;

        incoming_queue_.Swap(work_queue_);
        DCHECK(incoming_queue_.empty());
    }
}

void MessageLoop::deletePendingTasks()
{
    while (!work_queue_.empty())
        work_queue_.pop();

    while (!delayed_work_queue_.empty())
        delayed_work_queue_.pop();
}

// static
MessageLoop::TimePoint MessageLoop::calculateDelayedRuntime(const Milliseconds& delay)
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

                addToDelayedWorkQueue(pending_task);

                // If we changed the topmost task, then it is time to reschedule.
                if (reschedule)
                    pump_->scheduleDelayedWork(pending_task.delayed_run_time);
            }
            else
            {
                runTask(pending_task);
            }
        }
        while (!work_queue_.empty());
    }

    // Nothing happened.
    return false;
}

bool MessageLoop::doDelayedWork(TimePoint& next_delayed_work_time)
{
    if (!nestable_tasks_allowed_ || delayed_work_queue_.empty())
    {
        recent_time_ = next_delayed_work_time = TimePoint();
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
            next_delayed_work_time = next_run_time;
            return false;
        }
    }

    PendingTask pending_task = delayed_work_queue_.top();
    delayed_work_queue_.pop();

    if (!delayed_work_queue_.empty())
        next_delayed_work_time = delayed_work_queue_.top().delayed_run_time;

    runTask(pending_task);
    return true;
}

} // namespace base
