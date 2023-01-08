//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_THREADING_THREAD_H
#define BASE_THREADING_THREAD_H

#include "base/message_loop/message_loop.h"
#include "build/build_config.h"

#include <atomic>
#include <condition_variable>
#include <thread>

namespace base {

class Thread
{
public:
    Thread() = default;
    ~Thread();

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        // Called just prior to starting the message loop.
        virtual void onBeforeThreadRunning()
        {
            // Nothing
        }

        virtual void onThreadRunning(MessageLoop* message_loop);

        // Called just after the message loop ends.
        virtual void onAfterThreadRunning()
        {
            // Nothing
        }
    };

    // Starts the thread.
    void start(MessageLoop::Type message_loop_type, Delegate* delegate = nullptr);

    // Signals the thread to exit in the near future.
    void stopSoon();

    // Signals the thread to exit and returns once the thread has exited.  After
    // this method returns, the Thread object is completely reset and may be used
    // as if it were newly constructed (i.e., Start may be called again).
    // Stop may be called multiple times and is simply ignored if the thread is
    // already stopped.
    void stop();

    void join();

    bool isRunning() const { return running_; }
    MessageLoop* messageLoop() const { return message_loop_; }

    std::shared_ptr<TaskRunner> taskRunner() const
    {
        return message_loop_ ? message_loop_->taskRunner() : nullptr;
    }

#if defined(OS_WIN)
    uint32_t threadId() const { return thread_id_; }

    enum class Priority
    {
        ABOVE_NORMAL = THREAD_PRIORITY_ABOVE_NORMAL,
        BELOW_NORMAL = THREAD_PRIORITY_BELOW_NORMAL,
        HIGHEST      = THREAD_PRIORITY_HIGHEST,
        IDLE         = THREAD_PRIORITY_IDLE,
        LOWEST       = THREAD_PRIORITY_LOWEST,
        NORMAL       = THREAD_PRIORITY_NORMAL
    };

    bool setPriority(Priority priority);

#endif // defined(OS_WIN)

private:
    void threadMain(MessageLoop::Type message_loop_type);

    Delegate* delegate_ = nullptr;

    enum class State { STARTING, STARTED, STOPPING, STOPPED };

    std::atomic<State> state_ = State::STOPPED;

    std::thread thread_;

#if defined(OS_WIN)
    uint32_t thread_id_ = 0;
#endif // defined(OS_WIN)

    // True while inside of Run().
    bool running_ = false;
    std::mutex running_lock_;
    std::condition_variable running_event_;

    MessageLoop* message_loop_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(Thread);
};

} // namespace base

#endif // BASE_THREADING_THREAD_H
