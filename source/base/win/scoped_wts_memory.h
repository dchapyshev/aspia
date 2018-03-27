//
// PROJECT:         Aspia
// FILE:            base/win/scoped_wts_memory.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__WIN__SCOPED_WTS_MEMORY_H
#define _ASPIA_BASE__WIN__SCOPED_WTS_MEMORY_H

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

    Q_DISABLE_COPY(ScopedWtsMemory)
};

#endif // _ASPIA_BASE__WIN__SCOPED_WTS_MEMORY_H
