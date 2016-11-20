/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/scoped_wtsapi32_library.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_BASE__SCOPED_WTSAPI32_LIBRARY_H
#define _ASPIA_BASE__SCOPED_WTSAPI32_LIBRARY_H

#include <wtsapi32.h>

#include "base/scoped_native_library.h"
#include "base/logging.h"

class ScopedWtsApi32Library : public ScopedNativeLibrary
{
public:
    ScopedWtsApi32Library() :
        ScopedNativeLibrary("wtsapi32.dll")
    {
        enumerate_processes_func_ =
            reinterpret_cast<WTSENUMERATEPROCESSESW>(GetFunctionPointer("WTSEnumerateProcessesW"));
        CHECK(enumerate_processes_func_);

        free_memory_func_ =
            reinterpret_cast<WTSFREEMEMORY>(GetFunctionPointer("WTSFreeMemory"));
        CHECK(free_memory_func_);

        query_user_token_func_ =
            reinterpret_cast<WTSQUERYUSERTOKEN>(GetFunctionPointer("WTSQueryUserToken"));
        CHECK(query_user_token_func_);
    }

    ~ScopedWtsApi32Library()
    {
        // Nothing
    }

    BOOL WTSEnumerateProcessesW(HANDLE hServer,
                                DWORD Reserved,
                                DWORD Version,
                                PWTS_PROCESS_INFOW *ppProcessInfo,
                                DWORD *pCount)
    {
        return enumerate_processes_func_(hServer, Reserved, Version, ppProcessInfo, pCount);
    }

    VOID WTSFreeMemory(PVOID pMemory)
    {
        free_memory_func_(pMemory);
    }

    BOOL WTSQueryUserToken(ULONG SessionId, PHANDLE phToken)
    {
        return query_user_token_func_(SessionId, phToken);
    }

private:
    typedef BOOL(WINAPI *WTSENUMERATEPROCESSESW)(HANDLE, DWORD, DWORD, PWTS_PROCESS_INFOW*, DWORD*);
    typedef VOID(WINAPI *WTSFREEMEMORY)(PVOID);
    typedef BOOL(WINAPI *WTSQUERYUSERTOKEN)(ULONG, PHANDLE);

    WTSENUMERATEPROCESSESW enumerate_processes_func_ = nullptr;
    WTSFREEMEMORY free_memory_func_ = nullptr;
    WTSQUERYUSERTOKEN query_user_token_func_ = nullptr;
};

#endif // _ASPIA_BASE__SCOPED_WTSAPI32_LIBRARY_H
