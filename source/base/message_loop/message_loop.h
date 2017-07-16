//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/message_loop/message_loop.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_LOOP_H
#define _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_LOOP_H

#include "base/message_loop/message_pump_ui.h"
#include "base/message_loop/task.h"

#include <memory>
#include <mutex>

namespace aspia {

class MessageLoopProxy;
class MessageThread;

class MessageLoop : public MessagePump::Delegate
{
public:
    enum Type { TYPE_DEFAULT, TYPE_UI };

    using Dispatcher = MessagePumpDispatcher;

    explicit MessageLoop(Type type = TYPE_DEFAULT);
    virtual ~MessageLoop();

    Type type() const { return type_; }

    void Run(Dispatcher *dispatcher = nullptr);

    static MessageLoop* Current();

protected:
    friend class MessageLoopProxy;
    friend class MessageLoopThread;

    void PostTask(Task::Callback callback);
    Task::Callback QuitClosure();

    MessagePumpWin* pump_win() const;
    std::shared_ptr<MessageLoopProxy> message_loop_proxy() const { return proxy_; }

    // Runs the specified PendingTask.
    void RunTask(const Task& pending_task);

    // Adds the pending task to our incoming_queue_.
    //
    // Caller retains ownership of |pending_task|, but this function will
    // reset the value of pending_task->task.  This is needed to ensure
    // that the posting call stack does not retain pending_task->task
    // beyond this function call.
    void AddToIncomingQueue(Task* pending_task);

    // Load tasks from the incoming_queue_ into work_queue_ if the latter is
    // empty.  The former requires a lock to access, while the latter is directly
    // accessible on this thread.
    void ReloadWorkQueue();

    void DeletePendingTasks();

    // MessagePump::Delegate methods:
    bool DoWork() override;

    const Type type_;

    // A list of tasks that need to be processed by this instance.  Note that
    // this queue is only accessed (push/pop) by our current thread.
    TaskQueue work_queue_;

    // A recursion block that prevents accidentally running additional tasks when
    // insider a (accidentally induced?) nested message pump.
    bool nestable_tasks_allowed_ = true;

    std::shared_ptr<MessagePump> pump_;

    TaskQueue incoming_queue_;
    std::mutex incoming_queue_lock_;

    std::shared_ptr<MessageLoopProxy> proxy_;

private:
    void Quit();

    DISALLOW_COPY_AND_ASSIGN(MessageLoop);
};

class MessageLoopForUI : public MessageLoop
{
public:
    MessageLoopForUI() : MessageLoop(TYPE_UI) { }

    // Returns the MessageLoopUI of the current thread.
    static MessageLoopForUI* Current();

protected:
    MessagePumpForUI* pump_ui() const;
};

} // namespace aspia

#endif // _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_LOOP_H
