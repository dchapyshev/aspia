/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/lock.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "base/lock.h"

namespace aspia {

#if (_WIN32_WINNT >= 0x0600)

Lock::Lock() : lock_(SRWLOCK_INIT)
{
    // Nothing
}

Lock::~Lock()
{
    // Nothing
}

void Lock::lock()
{
    AcquireSRWLockExclusive(&lock_);
}

bool Lock::try_lock()
{
    return !!TryAcquireSRWLockExclusive(&lock_);
}

void Lock::unlock()
{
    ReleaseSRWLockExclusive(&lock_);
}

#else

Lock::Lock()
{
    InitializeCriticalSection(&cs_);
}

Lock::~Lock()
{
    DeleteCriticalSection(&cs_);
}

void Lock::lock()
{
    EnterCriticalSection(&cs_);
}

bool Lock::try_lock()
{
    return !!TryEnterCriticalSection(&cs_);
}

void Lock::unlock()
{
    LeaveCriticalSection(&cs_);
}

#endif // (_WIN32_WINNT >= 0x0600)

} // namespace aspia
