/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/mutex.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_BASE__MUTEX_H
#define _ASPIA_BASE__MUTEX_H

#include "aspia_config.h"

class Mutex
{
public:
    Mutex()
    {
        InitializeCriticalSection(&cs_);
    }

    ~Mutex()
    {
        DeleteCriticalSection(&cs_);
    }

    void lock()
    {
        EnterCriticalSection(&cs_);
    }

    bool try_lock()
    {
        return TryEnterCriticalSection(&cs_) != FALSE;
    }

    void unlock()
    {
        LeaveCriticalSection(&cs_);
    }

private:
    CRITICAL_SECTION cs_;
};

template<class T>
class LockGuard
{
public:
    LockGuard(T *mutex) : mutex_(mutex)
    {
        mutex_->lock();
    }

    virtual ~LockGuard()
    {
        mutex_->unlock();
    }

private:
    T *mutex_;
};

#endif // _ASPIA_BASE__MUTEX_H
