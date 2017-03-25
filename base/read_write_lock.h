//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/read_write_lock.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__READ_WRITE_LOCK_H
#define _ASPIA_BASE__READ_WRITE_LOCK_H

#include "aspia_config.h"

#include <stdint.h>

#include "base/macros.h"

namespace aspia {

class ReadWriteLock
{
public:
    ReadWriteLock();
    ~ReadWriteLock() = default;

    void ReadAcquire();
    void ReadRelease();

    void WriteAcquire();
    void WriteRelease();

private:
    using NativeHandle = SRWLOCK;

    NativeHandle native_handle_;

    DISALLOW_COPY_AND_ASSIGN(ReadWriteLock);
};

class AutoReadLock
{
public:
    explicit AutoReadLock(ReadWriteLock& locker) : locker_(locker)
    {
        locker_.ReadAcquire();
    }

    virtual ~AutoReadLock()
    {
        locker_.ReadRelease();
    }

private:
    ReadWriteLock& locker_;

    DISALLOW_COPY_AND_ASSIGN(AutoReadLock);
};

class AutoReadUnlock
{
public:
    explicit AutoReadUnlock(ReadWriteLock& locker) : locker_(locker)
    {
        locker_.ReadRelease();
    }

    virtual ~AutoReadUnlock()
    {
        locker_.ReadAcquire();
    }

private:
    ReadWriteLock& locker_;

    DISALLOW_COPY_AND_ASSIGN(AutoReadUnlock);
};

class AutoWriteLock
{
public:
    explicit AutoWriteLock(ReadWriteLock &locker) : locker_(locker)
    {
        locker_.WriteAcquire();
    }

    virtual ~AutoWriteLock()
    {
        locker_.WriteRelease();
    }

private:
    ReadWriteLock& locker_;

    DISALLOW_COPY_AND_ASSIGN(AutoWriteLock);
};

class AutoWriteUnlock
{
public:
    explicit AutoWriteUnlock(ReadWriteLock &locker) : locker_(locker)
    {
        locker_.WriteRelease();
    }

    virtual ~AutoWriteUnlock()
    {
        locker_.WriteAcquire();
    }

private:
    ReadWriteLock& locker_;

    DISALLOW_COPY_AND_ASSIGN(AutoWriteUnlock);
};

} // namespace aspia

#endif // _ASPIA_BASE__READ_WRITE_LOCK_H
