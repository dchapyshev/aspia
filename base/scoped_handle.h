/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/scoped_handle.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

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

    ~ScopedHandle()
    {
        close();
    }

    HANDLE get()
    {
        return handle_;
    }

    void set(HANDLE handle)
    {
        close();
        handle_ = handle;
    }

    HANDLE release()
    {
        HANDLE handle = handle_;
        handle_ = nullptr;
        return handle;
    }

    ScopedHandle& operator=(HANDLE handle)
    {
        set(handle);
        return *this;
    }

    operator HANDLE()
    {
        return handle_;
    }

private:
    void close()
    {
        if (handle_ && handle_ != INVALID_HANDLE_VALUE)
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
