//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/thread.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/thread.h"

#include <process.h>

#include "base/logging.h"

namespace aspia {

// static
UINT CALLBACK Thread::ThreadProc(LPVOID param)
{
    Thread* self = reinterpret_cast<Thread*>(param);

    self->Worker();
    self->active_ = false;

    _endthreadex(0);
    return 0;
}

Thread::Thread()
{
    active_ = false;
    terminating_ = false;
}

void Thread::Stop()
{
    if (!terminating_)
    {
        terminating_ = true;
        OnStop();
    }
}

bool Thread::IsTerminating() const
{
    return terminating_;
}

bool Thread::IsActive() const
{
    return active_;
}

void Thread::SetPriority(Priority value)
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
            DLOG(WARNING) << "Unexpected priority:" << static_cast<int>(value);
            priority = THREAD_PRIORITY_BELOW_NORMAL;
        }
        break;
    }

    BOOL result = ::SetThreadPriority(thread_, priority);
    DCHECK(result) << GetLastError();
}

void Thread::Start(Priority priority)
{
    if (active_)
    {
        DLOG(ERROR) << "Attempt to start an already running thread";
        return;
    }

    active_ = false;
    terminating_ = false;

    thread_ = reinterpret_cast<HANDLE>(_beginthreadex(nullptr,
                                                      0,
                                                      ThreadProc,
                                                      this,
                                                      0,
                                                      nullptr));
    CHECK(thread_) << errno;

    active_ = true;

    if (priority != Priority::Normal)
        SetPriority(priority);
}

void Thread::Wait() const
{
    if (active_)
    {
        // Если запущен, то ждем пока он завершит работу.
        WaitForSingleObject(thread_, INFINITE);
        active_ = false;
    }
}

// static
void Thread::Sleep(uint32_t delay_in_ms)
{
    ::Sleep(delay_in_ms);
}

} // namespace aspia
