//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/read_write_lock.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/read_write_lock.h"

namespace aspia {

ReadWriteLock::ReadWriteLock() :
    native_handle_(SRWLOCK_INIT)
{
    // Nothing
}

void ReadWriteLock::ReadAcquire()
{
    AcquireSRWLockShared(&native_handle_);
}

void ReadWriteLock::ReadRelease()
{
    ReleaseSRWLockShared(&native_handle_);
}

void ReadWriteLock::WriteAcquire()
{
    AcquireSRWLockExclusive(&native_handle_);
}

void ReadWriteLock::WriteRelease()
{
    ReleaseSRWLockExclusive(&native_handle_);
}

} // namespace aspia
