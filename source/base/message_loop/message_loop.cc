//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include <memory>

#include "base/message_loop/message_loop_proxy.h"
#include "base/message_loop/message_pump_default.h"
#include "base/message_loop/message_pump_qt.h"

namespace aspia {

static thread_local MessageLoop* message_loop_for_current_thread = nullptr;

// static
MessageLoop* MessageLoop::current()
{
    return message_loop_for_current_thread;
}

MessageLoop::MessageLoop(Type type)
    : type_(type)
{
    // Should only have one message loop per thread.
    Q_ASSERT(!current());

    message_loop_for_current_thread = this;

    proxy_.reset(new MessageLoopProxy(this));

    switch (type)
    {
        case Type::WIN:
            pump_ = std::make_unique<MessagePumpForWin>();
            break;

        case Type::IO:
            // TODO
            break;

        case Type::QT:
            pump_ = std::make_unique<MessagePumpForQt>();
            break;

        default:
            pump_ = std::make_unique<MessagePumpDefault>();
            break;
    }
}

MessageLoop::~MessageLoop()
{
    Q_ASSERT(this == current());

    reloadWorkQueue();
    deletePendingTasks();

    proxy_->willDestroyCurrentMessageLoop();
    proxy_ = nullptr;

    message_loop_for_current_thread = nullptr;
}

int MessageLoop::run(Dispatcher* dispatcher)
{
    Q_ASSERT(this == current());

    if (dispatcher && type() == Type::WIN)
        return pumpWin()->runWithDispatcher(this, dispatcher);

    return pump_->run(this);
}

void MessageLoop::quit()
{
    Q_ASSERT(this == current());
    pump_->quit();
}

PendingTask::Callback MessageLoop::quitClosure()
{
    return std::bind(&MessageLoop::quit, this);
}

void MessageLoop::postTask(PendingTask::Callback callback)
{
    Q_ASSERT(callback != nullptr);

    PendingTask pending_task(std::move(callback),
                             calculateDelayedRuntime(std::chrono::milliseconds::zero()));
    addToIncomingQueue(pending_task);
}

void MessageLoop::postDelayedTask(PendingTask::Callback callback,
                                  const std::chrono::milliseconds& delay)
{
    Q_ASSERT(callback != nullptr);

    PendingTask pending_task(std::move(callback), calculateDelayedRuntime(delay));
    addToIncomingQueue(pending_task);
}

MessagePumpForWin* MessageLoop::pumpWin() const
{
    return static_cast<MessagePumpForWin*>(pump_.get());
}

void MessageLoop::runTask(const PendingTask& pending_task)
{
    Q_ASSERT(nestable_tasks_allowed_);

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
        std::scoped_lock<std::mutex> lock(incoming_queue_lock_);

        bool empty = incoming_queue_.empty();

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
        std::scoped_lock<std::mutex> lock(incoming_queue_lock_);

        if (incoming_queue_.empty())
            return;

        incoming_queue_.Swap(work_queue_);
        Q_ASSERT(incoming_queue_.empty());
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
MessageLoopTimePoint MessageLoop::calculateDelayedRuntime(const std::chrono::milliseconds& delay)
{
    MessageLoopTimePoint delayed_run_time;

    if (delay > std::chrono::milliseconds::zero())
        delayed_run_time = MessageLoopClock::now() + delay;

    return delayed_run_time;
}

bool MessageLoop::doWork()
{
    if (!nestable_tasks_allowed_)
    {
        // Task can't be executed right now.
        return false;
    }

    reloadWorkQueue();

    if (work_queue_.empty())
        return false;

    do
    {
        PendingTask pending_task = work_queue_.front();
        work_queue_.pop();

        if (pending_task.delayed_run_time != MessageLoopTimePoint())
        {
            bool reschedule = delayed_work_queue_.empty();

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

    return true;
}

bool MessageLoop::doDelayedWork(MessageLoopTimePoint& next_delayed_work_time)
{
    if (!nestable_tasks_allowed_ || delayed_work_queue_.empty())
    {
        recent_time_ = next_delayed_work_time = MessageLoopTimePoint();
        return false;
    }

    // When we "fall behind," there will be a lot of tasks in the delayed work queue that are ready
    // to run.  To increase efficiency when we fall behind, we will only call Time::Now()
    // intermittently, and then process all tasks that are ready to run before calling it again.
    // As a result, the more we fall behind (and have a lot of ready-to-run delayed tasks), the more
    // efficient we'll be at handling the tasks.

    MessageLoopTimePoint next_run_time = delayed_work_queue_.top().delayed_run_time;
    if (next_run_time > recent_time_)
    {
        recent_time_ = MessageLoopClock::now();
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

std::shared_ptr<MessageLoopProxy> MessageLoop::messageLoopProxy() const
{
    return proxy_;
}

// static
MessageLoopForWin* MessageLoopForWin::current()
{
    MessageLoop* loop = MessageLoop::current();

    if (loop)
    {
        Q_ASSERT(MessageLoop::Type::WIN == loop->type());
    }

    return static_cast<MessageLoopForWin*>(loop);
}

MessagePumpForWin* MessageLoopForWin::pumpWin() const
{
    return static_cast<MessagePumpForWin*>(pump_.get());
}

} // namespace aspia
