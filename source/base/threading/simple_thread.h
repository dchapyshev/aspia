//
// PROJECT:         Aspia
// FILE:            base/threading/simple_thread.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__THREADING__SIMPLE_THREAD_H
#define _ASPIA_BASE__THREADING__SIMPLE_THREAD_H

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

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

protected:
    virtual void Run() = 0;

private:
    void ThreadMain();

    std::thread thread_;

    enum class State { STARTING, STARTED, STOPPING, STOPPED };

    std::atomic<State> state_ = State::STOPPED;

    // True while inside of Run().
    bool running_ = false;
    std::mutex running_lock_;
    std::condition_variable running_event_;

    Q_DISABLE_COPY(SimpleThread)
};

} // namespace aspia

#endif // _ASPIA_BASE__THREADING__SIMPLE_THREAD_H
