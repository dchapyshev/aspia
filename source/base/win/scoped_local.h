//
// PROJECT:         Aspia
// FILE:            base/win/scoped_local.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__WIN__SCOPED_LOCAL_H
#define _ASPIA_BASE__WIN__SCOPED_LOCAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace aspia {

template <typename T>
class ScopedLocal
{
public:
    ScopedLocal() = default;

    ScopedLocal(ScopedLocal&& other) noexcept
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

    ~ScopedLocal() { close(); }

    T get() { return local_; }

    void reset(T local = nullptr)
    {
        close();
        local_ = local;
    }

    T release()
    {
        T local = local_;
        local_ = nullptr;
        return local;
    }

    T* recieve()
    {
        close();
        return &local_;
    }

    bool isValid() const
    {
        return (local_ != nullptr);
    }

    ScopedLocal& operator=(ScopedLocal&& other) noexcept
    {
        close();
        local_ = other.local_;
        other.local_ = nullptr;
        return *this;
    }

    operator T() { return local_; }

private:
    void close()
    {
        if (isValid())
        {
            LocalFree(local_);
            local_ = nullptr;
        }
    }

    T local_ = nullptr;

    Q_DISABLE_COPY(ScopedLocal)
};

} // namespace aspia

#endif // _ASPIA_BASE__WIN__SCOPED_LOCAL_H
