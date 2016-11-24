/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/scoped_sc_handle.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_BASE__SCOPED_SC_HANDLE_H
#define _ASPIA_BASE__SCOPED_SC_HANDLE_H

#include "aspia_config.h"

#include "base/macros.h"

class ScopedScHandle
{
public:
    ScopedScHandle() : handle_(nullptr)
    {
        // Nothing
    }

    explicit ScopedScHandle(SC_HANDLE handle) : handle_(handle)
    {
        // Nothing
    }

    ~ScopedScHandle()
    {
        close();
    }

    SC_HANDLE get()
    {
        return handle_;
    }

    void set(SC_HANDLE handle)
    {
        close();
        handle_ = handle;
    }

    SC_HANDLE release()
    {
        SC_HANDLE handle = handle_;
        handle_ = nullptr;
        return handle;
    }

    ScopedScHandle& operator=(SC_HANDLE handle)
    {
        set(handle);
        return *this;
    }

    operator SC_HANDLE()
    {
        return handle_;
    }

private:
    void close()
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

#endif // _ASPIA_BASE__SCOPED_SC_HANDLE_H
