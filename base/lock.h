//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/lock.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__LOCK_H
#define _ASPIA_BASE__LOCK_H

#include "aspia_config.h"

#include <stdint.h>

#include "base/macros.h"

namespace aspia {

class Lock
{
public:
    using NativeHandle = SRWLOCK;

    Lock();
    ~Lock() = default;

    void Acquire();
    bool Try();
    void Release();

    NativeHandle* native_handle() { return &native_handle_; }

private:
    NativeHandle native_handle_;

    DISALLOW_COPY_AND_ASSIGN(Lock);
};

class AutoLock
{
public:
    explicit AutoLock(Lock& locker) : locker_(locker)
    {
        locker_.Acquire();
    }

    virtual ~AutoLock()
    {
        locker_.Release();
    }

private:
    Lock& locker_;

    DISALLOW_COPY_AND_ASSIGN(AutoLock);
};

class AutoUnlock
{
public:
    explicit AutoUnlock(Lock& locker) : locker_(locker)
    {
        locker_.Release();
    }

    virtual ~AutoUnlock()
    {
        locker_.Acquire();
    }

private:
    Lock& locker_;

    DISALLOW_COPY_AND_ASSIGN(AutoUnlock);
};

} // namespace aspia

#endif // _ASPIA_BASE__LOCK_H
