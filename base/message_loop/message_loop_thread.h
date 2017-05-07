//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/message_loop/message_loop_thread.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_LOOP_THREAD_H
#define _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_LOOP_THREAD_H

#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"

#include <atomic>
#include <condition_variable>
#include <thread>

namespace aspia {

class MessageLoopThread
{
public:
    MessageLoopThread();
    ~MessageLoopThread();

    class Delegate
    {
    public:
        // Called just prior to starting the message loop.
        virtual void OnBeforeThreadRunning() { }

        // Called just after the message loop ends.
        virtual void OnAfterThreadRunning() { }
    };

    // Starts the thread.
    void Start(MessageLoop::Type message_loop_type, Delegate* delegate = nullptr);

    // Signals the thread to exit in the near future.
    void StopSoon();

    // Signals the thread to exit and returns once the thread has exited.  After
    // this method returns, the Thread object is completely reset and may be used
    // as if it were newly constructed (i.e., Start may be called again).
    // Stop may be called multiple times and is simply ignored if the thread is
    // already stopped.
    void Stop();

    MessageLoop* message_loop() const
    {
        return message_loop_;
    }

    std::shared_ptr<MessageLoopProxy> message_loop_proxy() const
    {
        return message_loop_ ? message_loop_->message_loop_proxy() : nullptr;
    }

private:
    void ThreadMain(MessageLoop::Type message_loop_type);

    Delegate* delegate_;

    enum class State { Starting, Started, Stopping, Stopped };

    std::atomic<State> state_;

    std::thread thread_;

    // True while inside of Run().
    bool running_;
    std::mutex running_lock_;
    std::condition_variable running_event_;

    MessageLoop* message_loop_;

    DISALLOW_COPY_AND_ASSIGN(MessageLoopThread);
};

} // namespace aspia

#endif // _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_LOOP_THREAD_H
