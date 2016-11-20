/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/scoped_kernel32_library.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_BASE__SCOPED_KERNEL32_LIBRARY_H
#define _ASPIA_BASE__SCOPED_KERNEL32_LIBRARY_H

#include "base/scoped_native_library.h"
#include "base/logging.h"

class ScopedKernel32Library : public ScopedNativeLibrary
{
public:
    ScopedKernel32Library() :
        ScopedNativeLibrary("kernel32.dll")
    {
        wts_get_active_console_session_id_func_ =
            reinterpret_cast<WTSGETACTIVECONSOLESESSIONID>(GetFunctionPointer("WTSGetActiveConsoleSessionId"));
        CHECK(wts_get_active_console_session_id_func_);

        process_id_to_session_id_func_ =
            reinterpret_cast<PROCESSIDTOSESSIONID>(GetFunctionPointer("ProcessIdToSessionId"));
        CHECK(process_id_to_session_id_func_);
    }

    ~ScopedKernel32Library()
    {
        // Nothing
    }

    DWORD WTSGetActiveConsoleSessionId(VOID)
    {
        return wts_get_active_console_session_id_func_();
    }

    BOOL ProcessIdToSessionId(DWORD dwProcessId, DWORD *pSessionId)
    {
        return process_id_to_session_id_func_(dwProcessId, pSessionId);
    }

private:
    typedef DWORD(WINAPI *WTSGETACTIVECONSOLESESSIONID)(VOID);
    typedef BOOL(WINAPI *PROCESSIDTOSESSIONID)(DWORD, DWORD*);

    WTSGETACTIVECONSOLESESSIONID wts_get_active_console_session_id_func_ = nullptr;
    PROCESSIDTOSESSIONID process_id_to_session_id_func_ = nullptr;
};

#endif // _ASPIA_BASE__SCOPED_KERNEL32_LIBRARY_H
