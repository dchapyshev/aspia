//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/scoped_addrinfo.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SCOPED_ADDRINFO_H
#define _ASPIA_BASE__SCOPED_ADDRINFO_H

#include <winsock2.h>
#include <ws2tcpip.h>

#include "base/macros.h"

namespace aspia {

class ScopedAddrInfo
{
public:
    ScopedAddrInfo() = default;

    explicit ScopedAddrInfo(ADDRINFOW* ptr) : ptr_(ptr)
    {
        // Nothing
    }

    ScopedAddrInfo(ScopedAddrInfo&& other)
    {
        ptr_ = other.ptr_;
        other.ptr_ = nullptr;
    }

    ~ScopedAddrInfo()
    {
        Close();
    }

    ADDRINFOW* Get()
    {
        return ptr_;
    }

    void Set(ADDRINFOW* ptr)
    {
        Close();
        ptr_ = ptr;
    }

    ADDRINFOW** Recieve()
    {
        Close();
        return &ptr_;
    }

    ADDRINFOW* Release()
    {
        ADDRINFOW* ptr = ptr_;
        ptr_ = nullptr;
        return ptr;
    }

    bool IsValid() const
    {
        return (ptr_ != nullptr);
    }

    ScopedAddrInfo& operator=(ADDRINFOW* ptr)
    {
        Set(ptr);
        return *this;
    }

    ScopedAddrInfo& operator=(ScopedAddrInfo&& other)
    {
        Close();
        ptr_ = other.ptr_;
        other.ptr_ = nullptr;
        return *this;
    }

    operator ADDRINFOW*()
    {
        return ptr_;
    }

private:
    void Close()
    {
        if (IsValid())
        {
            FreeAddrInfoW(ptr_);
            ptr_ = nullptr;
        }
    }

private:
    ADDRINFOW* ptr_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(ScopedAddrInfo);
};

} // namespace aspia

#endif // _ASPIA_BASE__SCOPED_ADDRINFO_H
