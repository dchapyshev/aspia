//
// PROJECT:         Aspia
// FILE:            base/process/process_helpers.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/process/process_helpers.h"
#include "base/files/base_paths.h"
#include "base/scoped_object.h"
#include "base/scoped_local.h"
#include "base/logging.h"

#include <shellapi.h>
#include <memory>

namespace aspia {

namespace {

bool LaunchProcessInternal(const wchar_t* program_path, const wchar_t* arguments, bool elevate)
{
    SHELLEXECUTEINFOW sei;
    memset(&sei, 0, sizeof(sei));

    sei.cbSize       = sizeof(sei);
    sei.lpVerb       = elevate ? L"runas" : L"open";
    sei.lpFile       = program_path;
    sei.hwnd         = nullptr;
    sei.nShow        = SW_SHOW;
    sei.lpParameters = arguments;

    if (!ShellExecuteExW(&sei))
    {
        DPLOG(LS_WARNING) << "ShellExecuteExW failed";
        return false;
    }

    return true;
}

} // namespace

bool LaunchProcessWithElevate(const std::experimental::filesystem::path& program_path)
{
    return LaunchProcessInternal(program_path.c_str(), nullptr, true);
}

bool LaunchProcessWithElevate(const CommandLine& command_line)
{
    return LaunchProcessInternal(command_line.GetProgram().c_str(),
                                 command_line.GetArgumentsString().c_str(),
                                 true);
}

bool LaunchProcess(const std::experimental::filesystem::path& program_path)
{
    return LaunchProcessInternal(program_path.c_str(), nullptr, false);
}

bool LaunchProcess(const CommandLine& command_line)
{
    return LaunchProcessInternal(command_line.GetProgram().c_str(),
                                 command_line.GetArgumentsString().c_str(),
                                 false);
}

bool IsProcessElevated()
{
    ScopedHandle token;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, token.Recieve()))
    {
        PLOG(LS_ERROR) << "OpenProcessToken failed";
        return false;
    }

    TOKEN_ELEVATION elevation;
    DWORD size;

    if (!GetTokenInformation(token, TokenElevation, &elevation,
                             sizeof(elevation), &size))
    {
        PLOG(LS_ERROR) << "GetTokenInformation failed";
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
        PLOG(LS_ERROR) << "AllocateAndInitializeSid failed";
        return false;
    }

    BOOL is_admin = FALSE;

    if (!CheckTokenMembership(nullptr, admin_group, &is_admin))
    {
        PLOG(LS_ERROR) << "CheckTokenMembership failed";
        return false;
    }

    return !!is_admin;
}

bool IsCallerHasAdminRights()
{
    return IsProcessElevated();
}

} // namespace aspia
