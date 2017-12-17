//
// PROJECT:         Aspia
// FILE:            base/scoped_hglobal.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SCOPED_HGLOBAL_H
#define _ASPIA_BASE__SCOPED_HGLOBAL_H

#include "base/macros.h"

namespace aspia {

// Like ScopedHandle except for HGLOBAL.
template<class T>
class ScopedHGlobal
{
public:
    explicit ScopedHGlobal(HGLOBAL glob) : glob_(glob)
    {
        data_ = static_cast<T*>(GlobalLock(glob_));
    }

    ~ScopedHGlobal()
    {
        GlobalUnlock(glob_);
    }

    T* Get() { return data_; }

    size_t Size() const { return GlobalSize(glob_); }

    T* operator->() const
    {
        assert(data_ != 0);
        return data_;
    }

    T* Release()
    {
        T* data = data_;
        data_ = nullptr;
        return data;
    }

private:
    HGLOBAL glob_;

    T* data_;

    DISALLOW_COPY_AND_ASSIGN(ScopedHGlobal);
};

}  // namespace aspia

#endif // _ASPIA_BASE__SCOPED_HGLOBAL_H
