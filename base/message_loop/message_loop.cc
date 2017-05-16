//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/message_loop/message_loop.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/logging.h"

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
            pump_.reset(new MessagePumpForUI());
            break;

        case TYPE_IO:
            pump_.reset(new MessagePumpForIO());
            break;

        default:
            pump_.reset(new MessagePumpDefault());
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
        pump_win()->RunWithDispatcher(this, dispatcher);
        return;
    }

    pump_->Run(this);
}

void MessageLoop::Quit()
{
    DCHECK_EQ(this, Current());
    pump_->Quit();
}

Task::Callback MessageLoop::QuitClosure()
{
    return std::bind(&MessageLoop::Quit, this);
}

void MessageLoop::PostTask(Task::Callback callback)
{
    Task pending_task(std::move(callback));
    AddToIncomingQueue(&pending_task);
}

MessagePumpWin* MessageLoop::pump_win()
{
    return static_cast<MessagePumpWin*>(pump_.get());
}

void MessageLoop::RunTask(const Task& pending_task)
{
    DCHECK(nestable_tasks_allowed_);

    // Execute the task and assume the worst: It is probably not reentrant.
    nestable_tasks_allowed_ = false;

    pending_task.callback();

    nestable_tasks_allowed_ = true;
}

void MessageLoop::AddToIncomingQueue(Task* pending_task)
{
    std::shared_ptr<MessagePump> pump;

    {
        std::lock_guard<std::mutex> lock(incoming_queue_lock_);

        bool empty = incoming_queue_.empty();

        incoming_queue_.push(*pending_task);

        pending_task->callback = nullptr;

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
        std::lock_guard<std::mutex> lock(incoming_queue_lock_);

        if (incoming_queue_.empty())
            return;

        incoming_queue_.Swap(&work_queue_);
        DCHECK(incoming_queue_.empty());
    }
}

void MessageLoop::DeletePendingTasks()
{
    while (!work_queue_.empty())
        work_queue_.pop();
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
        RunTask(work_queue_.front());
        work_queue_.pop();
    }
    while (!work_queue_.empty());

    return true;
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

MessagePumpForUI* MessageLoopForUI::pump_ui()
{
    return static_cast<MessagePumpForUI*>(pump_.get());
}

// static
MessageLoopForIO* MessageLoopForIO::Current()
{
    MessageLoop* loop = MessageLoop::Current();

    if (loop)
    {
        DCHECK_EQ(MessageLoop::TYPE_IO, loop->type());
    }

    return static_cast<MessageLoopForIO*>(loop);
}

void MessageLoopForIO::RegisterIOHandler(HANDLE file_handle, IOHandler* handler)
{
    pump_io()->RegisterIOHandler(file_handle, handler);
}

bool MessageLoopForIO::WaitForIOCompletion(DWORD timeout, IOHandler* filter)
{
    return pump_io()->WaitForIOCompletion(timeout, filter);
}

MessagePumpForIO* MessageLoopForIO::pump_io()
{
    return static_cast<MessagePumpForIO*>(pump_.get());
}

} // namespace aspia
