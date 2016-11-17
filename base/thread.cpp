/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/thread.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "base/thread.h"

#include <process.h>

#include "base/exception.h"
#include "base/logging.h"

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
                                                      &thread_id_));
    if (!thread_)
    {
        LOG(ERROR) << "Unable to create thread: " << errno;
        throw Exception("Unable to create thread.");
    }
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

bool Thread::IsEndOfThread() const
{
    return end_;
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
            LOG(ERROR) << "Unknwon thread priority";
            throw Exception("Unknown thread priority passed to SetThreadPriority.");
        }
    }

    if (!::SetThreadPriority(thread_, priority))
    {
        LOG(ERROR) << "SetThreadPriority() failed: " << GetLastError();
        throw Exception("Unable to set thread priority.");
    }
}

Thread::Priority Thread::GetThreadPriority() const
{
    int value = ::GetThreadPriority(thread_);

    switch (value)
    {
        case THREAD_PRIORITY_IDLE:          return Priority::Idle;
        case THREAD_PRIORITY_LOWEST:        return Priority::Lowest;
        case THREAD_PRIORITY_BELOW_NORMAL:  return Priority::BelowNormal;
        case THREAD_PRIORITY_NORMAL:        return Priority::Normal;
        case THREAD_PRIORITY_ABOVE_NORMAL:  return Priority::AboveNormal;
        case THREAD_PRIORITY_HIGHEST:       return Priority::Highest;
        case THREAD_PRIORITY_TIME_CRITICAL: return Priority::TimeCritical;
    }

    LOG(WARNING) << "Unknown thread priority: " << value;
    throw Exception("Unknown thread priority.");
}

void Thread::Start()
{
    if (active_)
    {
        LOG(ERROR) << "Attempt to start an already running thread";
        throw Exception("Attempt to start an already running thread.");
    }

    OnStart();
    active_ = (ResumeThread(thread_) != -1);

    if (!active_)
    {
        LOG(ERROR) << "ResumeThread() failed: " << GetLastError();
        throw Exception("Unable to start thread.");
    }
}

void Thread::WaitForEnd(uint32_t milliseconds) const
{
    if (active_)
    {
        // ≈сли запущен, то ждем пока он завершит работу.
        DWORD error = WaitForSingleObject(thread_, milliseconds);

        switch (error)
        {
            case WAIT_TIMEOUT:
                LOG(WARNING) << "Timeout at end of thread";
                break;

            case WAIT_OBJECT_0:
                break;

            default:
                LOG(WARNING) << "Unknown error at end of thread: " << error;
                break;
        }
    }
}

void Thread::WaitForEnd() const
{
    if (active_)
    {
        // Если запущен, то ждем пока он завершит работу.
        WaitForSingleObject(thread_, INFINITE);
    }
}

// static
void Thread::Sleep(uint32_t milliseconds)
{
    ::Sleep(milliseconds);
}

uint32_t Thread::GetThreadId() const
{
    return thread_id_;
}
