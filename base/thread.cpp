/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/thread.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "base/thread.h"

#include <process.h>

#include "base/logging.h"

namespace aspia {

// static
UINT CALLBACK Thread::ThreadProc(LPVOID param)
{
    Thread *self = reinterpret_cast<Thread*>(param);

    self->Worker();
    self->active_ = false;

    _endthreadex(0);
    return 0;
}

Thread::Thread()
{
    active_ = false;
    end_ = false;

    thread_ = reinterpret_cast<HANDLE>(_beginthreadex(nullptr,
                                                      0,
                                                      ThreadProc,
                                                      this,
                                                      CREATE_SUSPENDED,
                                                      nullptr));

    CHECK(thread_) << "Unable to create thread: " << errno;
}

Thread::~Thread()
{
    // Nothing
}

void Thread::Stop()
{
    end_ = true;
    OnStop();
}

bool Thread::IsThreadTerminating() const
{
    return end_;
}

bool Thread::IsThreadTerminated() const
{
    return !active_;
}

void Thread::SetThreadPriority(Priority value)
{
    int priority;

    switch (value)
    {
        case Priority::Idle:         priority = THREAD_PRIORITY_IDLE;          break;
        case Priority::Lowest:       priority = THREAD_PRIORITY_LOWEST;        break;
        case Priority::BelowNormal:  priority = THREAD_PRIORITY_BELOW_NORMAL;  break;
        case Priority::Normal:       priority = THREAD_PRIORITY_NORMAL;        break;
        case Priority::AboveNormal:  priority = THREAD_PRIORITY_ABOVE_NORMAL;  break;
        case Priority::Highest:      priority = THREAD_PRIORITY_HIGHEST;       break;
        case Priority::TimeCritical: priority = THREAD_PRIORITY_TIME_CRITICAL; break;
        default:
        {
            LOG(WARNING) << "Unknwon thread priority";
            return;
        }
    }

    if (!::SetThreadPriority(thread_, priority))
    {
        LOG(WARNING) << "SetThreadPriority() failed: " << GetLastError();
    }
}

void Thread::Start()
{
    if (active_)
    {
        DLOG(ERROR) << "Attempt to start an already running thread";
        return;
    }

    active_ = (ResumeThread(thread_) != -1);

    if (!active_)
    {
        LOG(FATAL) << "ResumeThread() failed: " << GetLastError();
    }
}

void Thread::WaitForEnd() const
{
    if (active_)
    {
        // Если запущен, то ждем пока он завершит работу.
        if (WaitForSingleObject(thread_, INFINITE) == WAIT_FAILED)
        {
            DLOG(ERROR) << "WaitForSingleObject() failed: " << GetLastError();
        }
    }
}

} // namespace aspia
