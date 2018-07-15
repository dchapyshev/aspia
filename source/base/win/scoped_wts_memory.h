//
// PROJECT:         Aspia
// FILE:            base/win/scoped_wts_memory.h
// LICENSE:         GNU General Public License 3
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

    explicit ScopedWtsMemory(T memory)
        : memory_(memory)
    {
        // Nothing
    }

    ~ScopedWtsMemory() { close(); }

    T get() { return memory_; }

    void reset(T memory)
    {
        close();
        memory_ = memory;
    }

    T release()
    {
        T memory = memory_;
        memory_ = nullptr;
        return memory;
    }

    T* recieve()
    {
        close();
        return &memory_;
    }

    bool isValid() const
    {
        return (memory_ != nullptr);
    }

    operator T() { return memory_; }

    T operator [](DWORD index) const
    {
        return &memory_[index];
    }

private:
    void close()
    {
        if (isValid())
        {
            WTSFreeMemory(memory_);
            memory_ = nullptr;
        }
    }

    T memory_ = nullptr;

    Q_DISABLE_COPY(ScopedWtsMemory)
};

#endif // _ASPIA_BASE__WIN__SCOPED_WTS_MEMORY_H
