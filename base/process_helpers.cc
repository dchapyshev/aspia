//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/process_helpers.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/process_helpers.h"
#include "base/version_helpers.h"
#include "base/scoped_object.h"
#include "base/scoped_local.h"
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
        DLOG(WARNING) << "ShellExecuteExW() failed: "
                      << GetLastSystemErrorString();
        return false;
    }

    return true;
}

bool IsProcessElevated()
{
    ScopedHandle token;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, token.Recieve()))
    {
        LOG(ERROR) << "OpenProcessToken() failed: "
                   << GetLastSystemErrorString();
        return false;
    }

    TOKEN_ELEVATION elevation;
    DWORD size;

    if (!GetTokenInformation(token, TokenElevation, &elevation,
                             sizeof(elevation), &size))
    {
        LOG(ERROR) << "GetTokenInformation() failed: "
                   << GetLastSystemErrorString();
        return false;
    }

    return elevation.TokenIsElevated != 0;
}

bool IsCallerAdminGroupMember()
{
    SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
    ScopedLocal<PSID> admin_group;

    if (!AllocateAndInitializeSid(&nt_authority,
                                  2,
                                  SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_ADMINS,
                                  0, 0, 0, 0, 0, 0,
                                  admin_group.Recieve()))
    {
        LOG(ERROR) << "AllocateAndInitializeSid() failed: "
                   << GetLastSystemErrorString();
        return false;
    }

    BOOL is_admin = FALSE;

    if (!CheckTokenMembership(nullptr, admin_group, &is_admin))
    {
        LOG(ERROR) << "CheckTokenMembership() failed: "
                   << GetLastSystemErrorString();
        return false;
    }

    return !!is_admin;
}

bool IsCallerHasAdminRights()
{
    if (IsWindowsVistaOrGreater())
    {
        if (IsProcessElevated())
            return true;
    }
    else
    {
        if (IsCallerAdminGroupMember())
            return true;
    }

    return false;
}

} // namespace aspia
