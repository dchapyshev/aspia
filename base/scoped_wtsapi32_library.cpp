//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/scoped_wtsapi32_library.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/scoped_wtsapi32_library.h"

#include "base/logging.h"

namespace aspia {

#if (_WIN32_WINNT >= 0x0600)
ScopedWtsApi32Library::ScopedWtsApi32Library()
{
    enumerate_processes_func_ = ::WTSEnumerateProcessesW;
    free_memory_func_ = ::WTSFreeMemory;
    query_user_token_func_ = ::WTSQueryUserToken;
}
#else
    ScopedWtsApi32Library::ScopedWtsApi32Library() :
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
#endif // (_WIN32_WINNT >= 0x0600)

ScopedWtsApi32Library::~ScopedWtsApi32Library()
{
    // Nothing
}

BOOL ScopedWtsApi32Library::WTSEnumerateProcessesW(HANDLE hServer,
                                                   DWORD Reserved,
                                                   DWORD Version,
                                                   PWTS_PROCESS_INFOW *ppProcessInfo,
                                                   DWORD *pCount)
{
    return enumerate_processes_func_(hServer, Reserved, Version, ppProcessInfo, pCount);
}

VOID ScopedWtsApi32Library::WTSFreeMemory(PVOID pMemory)
{
    free_memory_func_(pMemory);
}

BOOL ScopedWtsApi32Library::WTSQueryUserToken(ULONG SessionId, PHANDLE phToken)
{
    return query_user_token_func_(SessionId, phToken);
}

} // namespace aspia
