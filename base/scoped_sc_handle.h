//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/scoped_sc_handle.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SCOPED_SC_HANDLE_H
#define _ASPIA_BASE__SCOPED_SC_HANDLE_H

#include "aspia_config.h"

#include "base/macros.h"

namespace aspia {

class ScopedScHandle
{
public:
    ScopedScHandle() :
        handle_(nullptr)
    {
        // Nothing
    }

    explicit ScopedScHandle(SC_HANDLE handle) :
        handle_(handle)
    {
        // Nothing
    }

    ScopedScHandle(ScopedScHandle&& other)
    {
        handle_ = other.handle_;
        other.handle_ = nullptr;
    }

    ~ScopedScHandle()
    {
        Close();
    }

    SC_HANDLE Get()
    {
        return handle_;
    }

    void Set(SC_HANDLE handle)
    {
        Close();
        handle_ = handle;
    }

    bool IsValid() const
    {
        return handle_ != nullptr;
    }

    SC_HANDLE Release()
    {
        SC_HANDLE handle = handle_;
        handle_ = nullptr;
        return handle;
    }

    ScopedScHandle& operator=(SC_HANDLE handle)
    {
        Set(handle);
        return *this;
    }

    ScopedScHandle& operator=(ScopedScHandle&& other)
    {
        Close();
        handle_ = other.handle_;
        other.handle_ = nullptr;
        return *this;
    }

    operator SC_HANDLE()
    {
        return handle_;
    }

private:
    void Close()
    {
        if (handle_)
        {
            CloseServiceHandle(handle_);
            handle_ = nullptr;
        }
    }

private:
    SC_HANDLE handle_;

    DISALLOW_COPY_AND_ASSIGN(ScopedScHandle);
};

} // namespace aspia

#endif // _ASPIA_BASE__SCOPED_SC_HANDLE_H
