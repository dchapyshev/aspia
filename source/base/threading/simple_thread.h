//
// PROJECT:         Aspia
// FILE:            base/threading/simple_thread.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__THREADING__SIMPLE_THREAD_H
#define _ASPIA_BASE__THREADING__SIMPLE_THREAD_H

#include <atomic>
#include <mutex>
#include <thread>

#include "base/synchronization/waitable_event.h"
#include "base/macros.h"

namespace aspia {

class SimpleThread
{
public:
    SimpleThread() = default;
    virtual ~SimpleThread() = default;

    // Starts the thread and waits for its real start
    void Start();

    // Signals the thread to exit in the near future.
    void StopSoon();

    // Signals the thread to exit and returns once the thread has exited.
    // After this method returns, the Thread object is completely reset and may
    // be used as if it were newly constructed (i.e., Start may be called
    // again).
    // Stop may be called multiple times and is simply ignored if the thread is
    // already stopped.
    void Stop();

    // Waits for thread to finish.
    void Join();

    // Returns true if the StopSoon method was called.
    bool IsStopping() const;

    // Returns if the Run method is running.
    bool IsRunning() const;

    enum class Priority
    {
        ABOVE_NORMAL = THREAD_PRIORITY_ABOVE_NORMAL,
        BELOW_NORMAL = THREAD_PRIORITY_BELOW_NORMAL,
        HIGHEST      = THREAD_PRIORITY_HIGHEST,
        IDLE         = THREAD_PRIORITY_IDLE,
        LOWEST       = THREAD_PRIORITY_LOWEST,
        NORMAL       = THREAD_PRIORITY_NORMAL
    };

    bool SetPriority(Priority priority);

protected:
    virtual void Run() = 0;

private:
    void ThreadMain();

    std::thread thread_;
    std::mutex thread_lock_;

    enum class State { STARTING, STARTED, STOPPING, STOPPED };

    std::atomic<State> state_ = State::STOPPED;

    // True while inside of Run().
    std::atomic_bool running_ = false;

    WaitableEvent start_event_ { WaitableEvent::ResetPolicy::MANUAL,
                                 WaitableEvent::InitialState::NOT_SIGNALED };

    DISALLOW_COPY_AND_ASSIGN(SimpleThread);
};

} // namespace aspia

#endif // _ASPIA_BASE__THREADING__SIMPLE_THREAD_H
