//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/scoped_kernel32_library.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/scoped_kernel32_library.h"

#include "base/logging.h"

namespace aspia {

#if (_WIN32_WINNT >= 0x0600)
ScopedKernel32Library::ScopedKernel32Library()
{
    wts_get_active_console_session_id_func_ = ::WTSGetActiveConsoleSessionId;
    process_id_to_session_id_func_ = ::ProcessIdToSessionId;
}
#else
    ScopedKernel32Library::ScopedKernel32Library() :
        ScopedNativeLibrary("kernel32.dll")
    {
        wts_get_active_console_session_id_func_ =
            reinterpret_cast<WTSGETACTIVECONSOLESESSIONID>(GetFunctionPointer("WTSGetActiveConsoleSessionId"));
        CHECK(wts_get_active_console_session_id_func_);

        process_id_to_session_id_func_ =
            reinterpret_cast<PROCESSIDTOSESSIONID>(GetFunctionPointer("ProcessIdToSessionId"));
        CHECK(process_id_to_session_id_func_);
    }
#endif // (_WIN32_WINNT >= 0x0600)

ScopedKernel32Library::~ScopedKernel32Library()
{
    // Nothing
}

DWORD ScopedKernel32Library::WTSGetActiveConsoleSessionId(VOID)
{
    return wts_get_active_console_session_id_func_();
}

BOOL ScopedKernel32Library::ProcessIdToSessionId(DWORD dwProcessId, DWORD *pSessionId)
{
    return process_id_to_session_id_func_(dwProcessId, pSessionId);
}

} // namespace aspia
