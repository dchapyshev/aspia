//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/version_helpers.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/version_helpers.h"

namespace aspia {

static bool IsWindowsVersionOrGreater(WORD major, WORD minor, WORD service_pack_major)
{
    OSVERSIONINFOEXW osvi = { 0 };
    osvi.dwOSVersionInfoSize = sizeof(osvi);

    DWORDLONG const condition_mask = VerSetConditionMask(
        VerSetConditionMask(
            VerSetConditionMask(
                0, VER_MAJORVERSION, VER_GREATER_EQUAL),
            VER_MINORVERSION, VER_GREATER_EQUAL),
        VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);

    osvi.dwMajorVersion    = major;
    osvi.dwMinorVersion    = minor;
    osvi.wServicePackMajor = service_pack_major;

    return VerifyVersionInfoW(&osvi,
                              VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR,
                              condition_mask) != FALSE;
}

bool IsWindowsVistaOrGreater()
{
    return IsWindowsVersionOrGreater(HIBYTE(_WIN32_WINNT_VISTA), LOBYTE(_WIN32_WINNT_VISTA), 0);
}

bool IsWindows7OrGreater()
{
    return IsWindowsVersionOrGreater(HIBYTE(_WIN32_WINNT_WIN7), LOBYTE(_WIN32_WINNT_WIN7), 0);
}

} // namespace aspia
