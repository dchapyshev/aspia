//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/scoped_local.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SCOPED_LOCAL_H
#define _ASPIA_BASE__SCOPED_LOCAL_H

#include "base/macros.h"

namespace aspia {

template <typename T>
class ScopedLocal
{
public:
    ScopedLocal() : local_(nullptr)
    {
        // Nothing
    }

    ScopedLocal(ScopedLocal&& other)
    {
        local_ = other.local_;
        other.local_ = nullptr;
    }

    explicit ScopedLocal(T local) : local_(local)
    {
        // Nothing
    }

    explicit ScopedLocal(SIZE_T length)
    {
        local_ = reinterpret_cast<T>(LocalAlloc(LHND, length));
    }

    ~ScopedLocal()
    {
        Close();
    }

    T Get()
    {
        return local_;
    }

    void Reset(T local = nullptr)
    {
        Close();
        local_ = local;
    }

    T Release()
    {
        T local = local_;
        local_ = nullptr;
        return local;
    }

    T* Recieve()
    {
        Close();
        return &local_;
    }

    bool IsValid() const
    {
        return (local_ != nullptr);
    }

    ScopedLocal& operator=(T other)
    {
        Set(other);
        return *this;
    }

    ScopedLocal& operator=(ScopedLocal&& other)
    {
        Close();
        local_ = other.local_;
        other.local_ = nullptr;
        return *this;
    }

    operator T()
    {
        return local_;
    }

private:
    void Close()
    {
        if (IsValid())
        {
            LocalFree(local_);
            local_ = nullptr;
        }
    }

private:
    T local_;

    DISALLOW_COPY_AND_ASSIGN(ScopedLocal);
};

} // namespace aspia

#endif // _ASPIA_BASE__SCOPED_LOCAL_H
