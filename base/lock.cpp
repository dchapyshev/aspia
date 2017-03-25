//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/lock.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/lock.h"

namespace aspia {

Lock::Lock() : native_handle_(SRWLOCK_INIT)
{
    // Nothing
}

void Lock::Acquire()
{
    AcquireSRWLockExclusive(&native_handle_);
}

bool Lock::Try()
{
    return !!TryAcquireSRWLockExclusive(&native_handle_);
}

void Lock::Release()
{
    ReleaseSRWLockExclusive(&native_handle_);
}

} // namespace aspia
