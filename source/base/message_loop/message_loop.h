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

#ifndef ASPIA_BASE__MESSAGE_LOOP__MESSAGE_LOOP_H_
#define ASPIA_BASE__MESSAGE_LOOP__MESSAGE_LOOP_H_

#include <memory>
#include <mutex>

#include "base/message_loop/message_pump_win.h"
#include "base/message_loop/pending_task.h"

namespace aspia {

class MessageLoopProxy;
class Thread;

class MessageLoop : public MessagePump::Delegate
{
public:
    enum class Type
    {
        DEFAULT,
#if defined(Q_OS_WIN)
        WIN,
#endif // defined(Q_OS_WIN)
        IO,
        QT
    };

    using Dispatcher = MessagePumpDispatcher;

    explicit MessageLoop(Type type = Type::DEFAULT);
    virtual ~MessageLoop();

    Type type() const { return type_; }

    int run(Dispatcher *dispatcher = nullptr);

    static MessageLoop* current();

protected:
    friend class MessageLoopProxy;
    friend class MessageLoopThread;

    void postTask(PendingTask::Callback callback);
    void postDelayedTask(PendingTask::Callback callback, const std::chrono::milliseconds& delay);

    PendingTask::Callback quitClosure();

    MessagePumpForWin* pumpWin() const;
    std::shared_ptr<MessageLoopProxy> messageLoopProxy() const;

    // Runs the specified PendingTask.
    void runTask(const PendingTask& pending_task);

    // Adds the pending task to delayed_work_queue_.
    void addToDelayedWorkQueue(const PendingTask& pending_task);

    // Adds the pending task to our incoming_queue_.
    //
    // Caller retains ownership of |pending_task|, but this function will reset the value of
    // pending_task->task. This is needed to ensure that the posting call stack does not retain
    // pending_task->task beyond this function call.
    void addToIncomingQueue(PendingTask& pending_task);

    // Load tasks from the incoming_queue_ into work_queue_ if the latter is empty. The former
    // requires a lock to access, while the latter is directly accessible on this thread.
    void reloadWorkQueue();

    void deletePendingTasks();

    // Calculates the time at which a PendingTask should run.
    static MessageLoopTimePoint calculateDelayedRuntime(const std::chrono::milliseconds& delay);

    // MessagePump::Delegate methods:
    bool doWork() override;
    bool doDelayedWork(MessageLoopTimePoint& next_delayed_work_time) override;

    const Type type_;

    // Contains delayed tasks, sorted by their 'delayed_run_time' property.
    DelayedTaskQueue delayed_work_queue_;

    // A recent snapshot of Time::Now(), used to check delayed_work_queue_.
    MessageLoopTimePoint recent_time_;

    // A list of tasks that need to be processed by this instance. Note that this queue is only
    // accessed (push/pop) by our current thread.
    TaskQueue work_queue_;

    // A recursion block that prevents accidentally running additional tasks when insider a
    // (accidentally induced?) nested message pump.
    bool nestable_tasks_allowed_ = true;

    std::shared_ptr<MessagePump> pump_;

    TaskQueue incoming_queue_;
    std::mutex incoming_queue_lock_;

    // The next sequence number to use for delayed tasks.
    int next_sequence_num_ = 0;

    std::shared_ptr<MessageLoopProxy> proxy_;

private:
    void quit();

    Q_DISABLE_COPY(MessageLoop)
};

class MessageLoopForWin : public MessageLoop
{
public:
    MessageLoopForWin()
        : MessageLoop(Type::WIN)
    {
        // Nothing
    }

    // Returns the MessageLoopUI of the current thread.
    static MessageLoopForWin* current();

protected:
    MessagePumpForWin* pumpWin() const;
};

} // namespace aspia

#endif // ASPIA_BASE__MESSAGE_LOOP__MESSAGE_LOOP_H_
