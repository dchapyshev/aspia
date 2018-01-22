//
// PROJECT:         Aspia
// FILE:            base/process/process_helpers.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/process/process_helpers.h"
#include "base/version_helpers.h"
#include "base/scoped_object.h"
#include "base/scoped_local.h"
#include "base/logging.h"
#include "base/files/base_paths.h"

#include <shellapi.h>
#include <memory>

namespace aspia {

bool ElevateProcess()
{
    std::experimental::filesystem::path path;

    if (!GetBasePath(BasePathKey::FILE_EXE, path))
        return false;

    SHELLEXECUTEINFOW sei;
    memset(&sei, 0, sizeof(sei));

    sei.cbSize       = sizeof(sei);
    sei.lpVerb       = L"runas";
    sei.lpFile       = path.c_str();
    sei.hwnd         = nullptr;
    sei.nShow        = SW_SHOW;
    sei.lpParameters = nullptr;

    if (!ShellExecuteExW(&sei))
    {
        DPLOG(LS_WARNING) << "ShellExecuteExW() failed";
        return false;
    }

    return true;
}

bool IsProcessElevated()
{
    ScopedHandle token;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, token.Recieve()))
    {
        PLOG(LS_ERROR) << "OpenProcessToken() failed";
        return false;
    }

    TOKEN_ELEVATION elevation;
    DWORD size;

    if (!GetTokenInformation(token, TokenElevation, &elevation,
                             sizeof(elevation), &size))
    {
        PLOG(LS_ERROR) << "GetTokenInformation() failed";
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
        PLOG(LS_ERROR) << "AllocateAndInitializeSid() failed";
        return false;
    }

    BOOL is_admin = FALSE;

    if (!CheckTokenMembership(nullptr, admin_group, &is_admin))
    {
        PLOG(LS_ERROR) << "CheckTokenMembership() failed";
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

bool IsRunningAsService()
{
    ScopedScHandle sc_manager(OpenSCManagerW(nullptr,
                                             SERVICES_ACTIVE_DATABASE,
                                             SC_MANAGER_ENUMERATE_SERVICE));

    if (!sc_manager.IsValid())
    {
        PLOG(LS_ERROR) << "OpenSCManagerW() failed";
        return false;
    }

    DWORD needed_bytes = 0;
    DWORD services_count = 0;

    if (!EnumServicesStatusExW(sc_manager,
                               SC_ENUM_PROCESS_INFO,
                               SERVICE_WIN32,
                               SERVICE_ACTIVE,
                               nullptr,
                               0,
                               &needed_bytes,
                               &services_count,
                               nullptr,
                               nullptr))
    {
        DWORD error_code = GetLastError();

        if (error_code != ERROR_MORE_DATA)
        {
            LOG(LS_ERROR) << "EnumServicesStatusExW() failed: "
                          << SystemErrorCodeToString(error_code);
            return false;
        }
    }
    else
    {
        LOG(LS_ERROR) << "EnumServicesStatusExW() returns unexpected value";
        return false;
    }

    std::unique_ptr<uint8_t[]> buffer =
        std::make_unique<uint8_t[]>(needed_bytes);

    if (!EnumServicesStatusExW(sc_manager,
                               SC_ENUM_PROCESS_INFO,
                               SERVICE_WIN32,
                               SERVICE_ACTIVE,
                               buffer.get(),
                               needed_bytes,
                               &needed_bytes,
                               &services_count,
                               nullptr,
                               nullptr))
    {
        PLOG(LS_ERROR) << "EnumServicesStatusExW() failed";
        return false;
    }

    LPENUM_SERVICE_STATUS_PROCESS services =
        reinterpret_cast<LPENUM_SERVICE_STATUS_PROCESS>(buffer.get());

    DWORD current_process_id = GetCurrentProcessId();

    for (DWORD i = 0; i < services_count; ++i)
    {
        if (services[i].ServiceStatusProcess.dwProcessId == current_process_id)
            return true;
    }

    return false;
}

} // namespace aspia
