//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/scoped_handle.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SCOPED_HANDLE_H
#define _ASPIA_BASE__SCOPED_HANDLE_H

#include "aspia_config.h"

#include "base/macros.h"
#include "base/logging.h"

namespace aspia {

class ScopedHandle
{
public:
    ScopedHandle() : handle_(nullptr)
    {
        // Nothing
    }

    explicit ScopedHandle(HANDLE handle) : handle_(handle)
    {
        // Nothing
    }

    ScopedHandle(ScopedHandle&& other)
    {
        handle_ = other.handle_;
        other.handle_ = nullptr;
    }

    ~ScopedHandle()
    {
        Close();
    }

    HANDLE Get()
    {
        return handle_;
    }

    void Set(HANDLE handle)
    {
        Close();
        handle_ = handle;
    }

    HANDLE* Recieve()
    {
        Close();
        return &handle_;
    }

    HANDLE Release()
    {
        HANDLE handle = handle_;
        handle_ = nullptr;
        return handle;
    }

    bool IsValid() const
    {
        return ((handle_ != nullptr) && (handle_ != INVALID_HANDLE_VALUE));
    }

    ScopedHandle& operator=(HANDLE handle)
    {
        Set(handle);
        return *this;
    }

    ScopedHandle& operator=(ScopedHandle&& other)
    {
        Close();
        handle_ = other.handle_;
        other.handle_ = nullptr;
        return *this;
    }

    operator HANDLE()
    {
        return handle_;
    }

private:
    void Close()
    {
        if (IsValid())
        {
            if (!CloseHandle(handle_))
            {
                DLOG(ERROR) << "CloseHandle() failed: " << GetLastError();
            }

            handle_ = nullptr;
        }
    }

private:
    HANDLE handle_;

    DISALLOW_COPY_AND_ASSIGN(ScopedHandle);
};

} // namespace aspia

#endif // _ASPIA_BASE__SCOPED_HANDLE_H
