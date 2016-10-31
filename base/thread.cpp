/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/thread.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "base/thread.h"

UINT CALLBACK Thread::ThreadProc(LPVOID param)
{
    Thread *self = reinterpret_cast<Thread*>(param);

    CHECK(self);
    CHECK(self->thread_);

    self->Worker();
    self->started_ = false;

    _endthreadex(0);
    return 0;
}

Thread::Thread()
{
    started_ = false;

    thread_ = reinterpret_cast<HANDLE>(_beginthreadex(nullptr,
                                                      0,
                                                      ThreadProc,
                                                      this,
                                                      CREATE_SUSPENDED,
                                                      &thread_id_));
    if (!thread_)
    {
        LOG(ERROR) << "Unable to create thread: " << errno;
    }
}

Thread::~Thread()
{
    if (thread_)
        CloseHandle(thread_);
}

bool Thread::IsValid() const
{
    return (thread_ != 0);
}

bool Thread::Stop()
{
    if (started_ && thread_)
    {
        started_ = false;
        OnStop();

        return true;
    }

    return false;
}

bool Thread::IsEndOfThread() const
{
    return !started_;
}

bool Thread::SetThreadPriority(Priority value)
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
            return false;
        }
    }

    if (!::SetThreadPriority(thread_, priority))
    {
        LOG(ERROR) << "SetThreadPriority() failed: " << GetLastError();
        return false;
    }

    return true;
}

Thread::Priority Thread::GetThreadPriority() const
{
    Priority priority;
    int value;

    value = ::GetThreadPriority(thread_);

    switch (value)
    {
        case THREAD_PRIORITY_IDLE:          priority = Priority::Idle;         break;
        case THREAD_PRIORITY_LOWEST:        priority = Priority::Lowest;       break;
        case THREAD_PRIORITY_BELOW_NORMAL:  priority = Priority::BelowNormal;  break;
        case THREAD_PRIORITY_NORMAL:        priority = Priority::Normal;       break;
        case THREAD_PRIORITY_ABOVE_NORMAL:  priority = Priority::AboveNormal;  break;
        case THREAD_PRIORITY_HIGHEST:       priority = Priority::Highest;      break;
        case THREAD_PRIORITY_TIME_CRITICAL: priority = Priority::TimeCritical; break;
        default:
        {
            LOG(WARNING) << "Unknown thread priority: " << value;
            priority = Priority::Unknown;
        }
        break;
    }

    return priority;
}

bool Thread::Start()
{
    if (!started_ && thread_)
    {
        OnStart();
        started_ = (ResumeThread(thread_) != -1);
    }

    return started_;
}

bool Thread::WaitForEnd(uint32_t milliseconds) const
{
    if (!thread_)
        return false;

    // ≈сли поток не был запущен, то возвращаем true.
    if (!started_)
        return true;

    // ≈сли запущен, то ждем пока он завершит работу.
    DWORD error = WaitForSingleObject(thread_, milliseconds);
    bool result = false;

    switch (error)
    {
        case WAIT_TIMEOUT:
            LOG(WARNING) << "Timeout at end of thread.";
            break;

        case WAIT_OBJECT_0:
            LOG(INFO) << "Thread completed successfully.";
            result = true;
            break;

        default:
            LOG(WARNING) << "Unknown error at end of thread: " << error;
            break;
    }

    return result;
}

bool Thread::WaitForEnd() const
{
    if (!thread_)
        return false;

    // ≈сли поток не был запущен, то возвращаем true.
    if (!started_)
        return true;

    // ≈сли запущен, то ждем пока он завершит работу.
    return (WaitForSingleObject(thread_, INFINITE) != WAIT_FAILED);
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
