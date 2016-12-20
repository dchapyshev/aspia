/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/scoped_kernel32_library.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_BASE__SCOPED_KERNEL32_LIBRARY_H
#define _ASPIA_BASE__SCOPED_KERNEL32_LIBRARY_H

#include "aspia_config.h"

#include "base/scoped_native_library.h"
#include "base/macros.h"

namespace aspia {

class ScopedKernel32Library
#if (_WIN32_WINNT < 0x0600)
    : private ScopedNativeLibrary
#endif // (_WIN32_WINNT < 0x0600)
{
public:
    ScopedKernel32Library();
    ~ScopedKernel32Library();

    DWORD WTSGetActiveConsoleSessionId(VOID);
    BOOL ProcessIdToSessionId(DWORD dwProcessId, DWORD *pSessionId);

private:
    typedef DWORD(WINAPI *WTSGETACTIVECONSOLESESSIONID)(VOID);
    typedef BOOL(WINAPI *PROCESSIDTOSESSIONID)(DWORD, DWORD*);

    WTSGETACTIVECONSOLESESSIONID wts_get_active_console_session_id_func_ = nullptr;
    PROCESSIDTOSESSIONID process_id_to_session_id_func_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(ScopedKernel32Library);
};

} // namespace aspia

#endif // _ASPIA_BASE__SCOPED_KERNEL32_LIBRARY_H
