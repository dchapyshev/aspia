//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/elevation_helpers.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/elevation_helpers.h"
#include "base/scoped_object.h"
#include "base/logging.h"
#include "base/path.h"

#include <shellapi.h>

namespace aspia {

bool ElevateProcess()
{
    std::wstring path;

    if (!GetPathW(PathKey::FILE_EXE, path))
        return false;

    SHELLEXECUTEINFOW sei = { 0 };

    sei.cbSize       = sizeof(SHELLEXECUTEINFOW);
    sei.lpVerb       = L"runas";
    sei.lpFile       = path.c_str();
    sei.hwnd         = nullptr;
    sei.nShow        = SW_SHOW;
    sei.lpParameters = nullptr;

    if (!ShellExecuteExW(&sei))
    {
        DLOG(WARNING) << "ShellExecuteExW() failed: " << GetLastError();
        return false;
    }

    return true;
}

bool IsProcessElevated()
{
    ScopedHandle token;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, token.Recieve()))
    {
        LOG(ERROR) << "OpenProcessToken() failed: " << GetLastError();
        return false;
    }

    TOKEN_ELEVATION elevation;
    DWORD size;

    if (!GetTokenInformation(token, TokenElevation, &elevation,
                             sizeof(elevation), &size))
    {
        LOG(ERROR) << "GetTokenInformation() failed: " << GetLastError();
        return false;
    }

    return elevation.TokenIsElevated != 0;
}

} // namespace aspia
