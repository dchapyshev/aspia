//
// PROJECT:         Aspia
// FILE:            base/message_loop/message_loop.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/message_loop/message_pump_default.h"
#include "base/logging.h"

#include <memory>

namespace aspia {

static thread_local MessageLoop* message_loop_for_current_thread = nullptr;

// static
MessageLoop* MessageLoop::Current()
{
    return message_loop_for_current_thread;
}

MessageLoop::MessageLoop(Type type) :
    type_(type)
{
    DCHECK(!Current()) << "should only have one message loop per thread";

    message_loop_for_current_thread = this;

    proxy_.reset(new MessageLoopProxy(this));

    switch (type)
    {
        case TYPE_UI:
            pump_ = std::make_unique<MessagePumpForUI>();
            break;

        default:
            pump_ = std::make_unique<MessagePumpDefault>();
            break;
    }
}

MessageLoop::~MessageLoop()
{
    DCHECK_EQ(this, Current());

    ReloadWorkQueue();
    DeletePendingTasks();

    proxy_->WillDestroyCurrentMessageLoop();
    proxy_ = nullptr;

    message_loop_for_current_thread = nullptr;
}

void MessageLoop::Run(Dispatcher *dispatcher)
{
    DCHECK_EQ(this, Current());

    if (dispatcher && type() == TYPE_UI)
    {
        pump_ui()->RunWithDispatcher(this, dispatcher);
        return;
    }

    pump_->Run(this);
}

void MessageLoop::Quit()
{
    DCHECK_EQ(this, Current());
    pump_->Quit();
}

PendingTask::Callback MessageLoop::QuitClosure()
{
    return std::bind(&MessageLoop::Quit, this);
}

void MessageLoop::PostTask(PendingTask::Callback callback)
{
    DCHECK(callback != nullptr);

    PendingTask pending_task(std::move(callback),
                             CalculateDelayedRuntime(PendingTask::TimeDelta::zero()));
    AddToIncomingQueue(pending_task);
}

void MessageLoop::PostDelayedTask(PendingTask::Callback callback,
                                  const PendingTask::TimeDelta& delay)
{
    DCHECK(callback != nullptr);

    PendingTask pending_task(std::move(callback), CalculateDelayedRuntime(delay));
    AddToIncomingQueue(pending_task);
}

MessagePumpForUI* MessageLoop::pump_ui() const
{
    return static_cast<MessagePumpForUI*>(pump_.get());
}

void MessageLoop::RunTask(const PendingTask& pending_task)
{
    DCHECK(nestable_tasks_allowed_);

    // Execute the task and assume the worst: It is probably not reentrant.
    nestable_tasks_allowed_ = false;

    pending_task.callback();

    nestable_tasks_allowed_ = true;
}

void MessageLoop::AddToDelayedWorkQueue(const PendingTask& pending_task)
{
    // Move to the delayed work queue.  Initialize the sequence number before inserting into the
    // delayed_work_queue_. The sequence number is used to faciliate FIFO sorting when two tasks
    // have the same delayed_run_time value.
    PendingTask new_pending_task(pending_task);
    new_pending_task.sequence_num = next_sequence_num_++;
    delayed_work_queue_.emplace(new_pending_task);
}

void MessageLoop::AddToIncomingQueue(PendingTask& pending_task)
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

    pump->ScheduleWork();
}

void MessageLoop::ReloadWorkQueue()
{
    if (!work_queue_.empty())
        return;

    {
        std::scoped_lock<std::mutex> lock(incoming_queue_lock_);

        if (incoming_queue_.empty())
            return;

        incoming_queue_.Swap(work_queue_);
        DCHECK(incoming_queue_.empty());
    }
}

void MessageLoop::DeletePendingTasks()
{
    while (!work_queue_.empty())
        work_queue_.pop();

    while (!delayed_work_queue_.empty())
        delayed_work_queue_.pop();
}

// static
PendingTask::TimePoint MessageLoop::CalculateDelayedRuntime(const PendingTask::TimeDelta& delay)
{
    PendingTask::TimePoint delayed_run_time;

    if (delay > PendingTask::TimeDelta::zero())
    {
        delayed_run_time = std::chrono::high_resolution_clock::now() + delay;
    }

    return delayed_run_time;
}

bool MessageLoop::DoWork()
{
    if (!nestable_tasks_allowed_)
    {
        // Task can't be executed right now.
        return false;
    }

    ReloadWorkQueue();

    if (work_queue_.empty())
        return false;

    do
    {
        PendingTask pending_task = work_queue_.front();
        work_queue_.pop();

        if (pending_task.delayed_run_time != PendingTask::TimePoint())
        {
            bool reschedule = delayed_work_queue_.empty();

            AddToDelayedWorkQueue(pending_task);

            // If we changed the topmost task, then it is time to reschedule.
            if (reschedule)
                pump_->ScheduleDelayedWork(pending_task.delayed_run_time);
        }
        else
        {
            RunTask(pending_task);
        }
    }
    while (!work_queue_.empty());

    return true;
}

bool MessageLoop::DoDelayedWork(PendingTask::TimePoint& next_delayed_work_time)
{
    if (!nestable_tasks_allowed_ || delayed_work_queue_.empty())
    {
        recent_time_ = next_delayed_work_time = PendingTask::TimePoint();
        return false;
    }

    // When we "fall behind," there will be a lot of tasks in the delayed work queue that are ready
    // to run.  To increase efficiency when we fall behind, we will only call Time::Now()
    // intermittently, and then process all tasks that are ready to run before calling it again.
    // As a result, the more we fall behind (and have a lot of ready-to-run delayed tasks), the more
    // efficient we'll be at handling the tasks.

    PendingTask::TimePoint next_run_time = delayed_work_queue_.top().delayed_run_time;

    if (next_run_time > recent_time_)
    {
        recent_time_ = std::chrono::high_resolution_clock::now();
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

    RunTask(pending_task);
    return true;
}

std::shared_ptr<MessageLoopProxy> MessageLoop::message_loop_proxy() const
{
    return proxy_;
}

// static
MessageLoopForUI* MessageLoopForUI::Current()
{
    MessageLoop* loop = MessageLoop::Current();

    if (loop)
    {
        DCHECK_EQ(MessageLoop::TYPE_UI, loop->type());
    }

    return static_cast<MessageLoopForUI*>(loop);
}

MessagePumpForUI* MessageLoopForUI::pump_ui() const
{
    return static_cast<MessagePumpForUI*>(pump_.get());
}

} // namespace aspia
