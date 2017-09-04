//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/scoped_wts_memory.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SCOPED_WTS_MEMORY_H
#define _ASPIA_BASE__SCOPED_WTS_MEMORY_H

#include "base/macros.h"

#include <wtsapi32.h>

template <typename T>
class ScopedWtsMemory
{
public:
    ScopedWtsMemory() = default;

    explicit ScopedWtsMemory(T memory) :
        memory_(memory)
    {
        // Nothing
    }

    ~ScopedWtsMemory()
    {
        Close();
    }

    T Get()
    {
        return memory_;
    }

    void Set(T memory)
    {
        Close();
        memory_ = memory;
    }

    T Release()
    {
        T memory = memory_;
        memory_ = nullptr;
        return memory;
    }

    T* Recieve()
    {
        Close();
        return &memory_;
    }

    bool IsValid() const
    {
        return (memory_ != nullptr);
    }

    ScopedWtsMemory& operator=(T other)
    {
        Set(other);
        return *this;
    }

    operator T()
    {
        return memory_;
    }

    T operator [](DWORD index) const
    {
        return &memory_[index];
    }

private:
    void Close()
    {
        if (IsValid())
        {
            WTSFreeMemory(memory_);
            memory_ = nullptr;
        }
    }

private:
    T memory_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(ScopedWtsMemory);
};

#endif // _ASPIA_BASE__SCOPED_WTS_MEMORY_H
