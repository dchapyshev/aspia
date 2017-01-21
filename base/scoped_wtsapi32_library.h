//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/scoped_wtsapi32_library.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SCOPED_WTSAPI32_LIBRARY_H
#define _ASPIA_BASE__SCOPED_WTSAPI32_LIBRARY_H

#include "aspia_config.h"

#include <wtsapi32.h>

#include "base/scoped_native_library.h"
#include "base/macros.h"

namespace aspia {


class ScopedWtsApi32Library
#if (_WIN32_WINNT < 0x0600)
    : private ScopedNativeLibrary
#endif // (_WIN32_WINNT < 0x0600)
{
public:
    ScopedWtsApi32Library();
    ~ScopedWtsApi32Library();

    BOOL WTSEnumerateProcessesW(HANDLE hServer,
                                DWORD Reserved,
                                DWORD Version,
                                PWTS_PROCESS_INFOW *ppProcessInfo,
                                DWORD *pCount);

    VOID WTSFreeMemory(PVOID pMemory);
    BOOL WTSQueryUserToken(ULONG SessionId, PHANDLE phToken);

private:
    typedef BOOL(WINAPI *WTSENUMERATEPROCESSESW)(HANDLE, DWORD, DWORD, PWTS_PROCESS_INFOW*, DWORD*);
    typedef VOID(WINAPI *WTSFREEMEMORY)(PVOID);
    typedef BOOL(WINAPI *WTSQUERYUSERTOKEN)(ULONG, PHANDLE);

    WTSENUMERATEPROCESSESW enumerate_processes_func_ = nullptr;
    WTSFREEMEMORY free_memory_func_ = nullptr;
    WTSQUERYUSERTOKEN query_user_token_func_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(ScopedWtsApi32Library);
};

} // namespace aspia

#endif // _ASPIA_BASE__SCOPED_WTSAPI32_LIBRARY_H
