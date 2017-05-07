//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/scoped_addrinfo.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SCOPED_ADDRINFO_H
#define _ASPIA_BASE__SCOPED_ADDRINFO_H

#include <winsock2.h>
#include <ws2tcpip.h>

#include "base/macros.h"
#include "base/logging.h"

namespace aspia {

class ScopedAddrInfo
{
public:
    ScopedAddrInfo() : ptr_(nullptr)
    {
        // Nothing
    }

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
    ADDRINFOW* ptr_;

    DISALLOW_COPY_AND_ASSIGN(ScopedAddrInfo);
};

} // namespace aspia

#endif // _ASPIA_BASE__SCOPED_ADDRINFO_H
