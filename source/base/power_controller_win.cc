//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "base/power_controller.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wtsapi32.h>

#include "base/win/scoped_object.h"
#include "base/logging.h"

namespace base {

namespace {

// Delay for shutdown and reboot.
const DWORD kActionDelayInSeconds = 30;

bool copyProcessToken(DWORD desired_access, win::ScopedHandle* token_out)
{
    win::ScopedHandle process_token;
    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_DUPLICATE | desired_access,
                          process_token.recieve()))
    {
        PLOG(LS_WARNING) << "OpenProcessToken failed";
        return false;
    }

    if (!DuplicateTokenEx(process_token,
                          desired_access,
                          nullptr,
                          SecurityImpersonation,
                          TokenPrimary,
                          token_out->recieve()))
    {
        PLOG(LS_WARNING) << "DuplicateTokenEx failed";
        return false;
    }

    return true;
}

// Creates a copy of the current process with SE_SHUTDOWN_NAME privilege enabled.
bool createPrivilegedToken(win::ScopedHandle* token_out)
{
    const DWORD desired_access = TOKEN_ADJUST_PRIVILEGES | TOKEN_IMPERSONATE |
        TOKEN_DUPLICATE | TOKEN_QUERY;

    win::ScopedHandle privileged_token;
    if (!copyProcessToken(desired_access, &privileged_token))
        return false;

    // Get the LUID for the SE_SHUTDOWN_NAME privilege.
    TOKEN_PRIVILEGES state;
    state.PrivilegeCount = 1;
    state.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!LookupPrivilegeValueW(nullptr, SE_SHUTDOWN_NAME, &state.Privileges[0].Luid))
    {
        PLOG(LS_WARNING) << "LookupPrivilegeValueW failed";
        return false;
    }

    // Enable the SE_SHUTDOWN_NAME privilege.
    if (!AdjustTokenPrivileges(privileged_token, FALSE, &state, 0, nullptr, nullptr))
    {
        PLOG(LS_WARNING) << "AdjustTokenPrivileges failed";
        return false;
    }

    token_out->reset(privileged_token.release());
    return true;
}

} // namespace

// static
bool PowerController::shutdown()
{
    const DWORD desired_access =
        TOKEN_ADJUST_DEFAULT | TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY;

    win::ScopedHandle process_token;
    if (!copyProcessToken(desired_access, &process_token))
        return false;

    win::ScopedHandle privileged_token;
    if (!createPrivilegedToken(&privileged_token))
        return false;

    if (!ImpersonateLoggedOnUser(privileged_token))
    {
        PLOG(LS_WARNING) << "ImpersonateLoggedOnUser failed";
        return false;
    }

    const DWORD reason = SHTDN_REASON_MAJOR_APPLICATION | SHTDN_REASON_MINOR_MAINTENANCE;

    bool result = !!InitiateSystemShutdownExW(nullptr,
                                              nullptr,
                                              kActionDelayInSeconds,
                                              TRUE,  // Force close apps.
                                              FALSE, // Shutdown.
                                              reason);
    if (!result)
        PLOG(LS_WARNING) << "InitiateSystemShutdownExW failed";

    BOOL ret = RevertToSelf();
    CHECK(ret);

    return result;
}

// static
bool PowerController::reboot()
{
    const DWORD desired_access =
        TOKEN_ADJUST_DEFAULT | TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY;

    win::ScopedHandle process_token;
    if (!copyProcessToken(desired_access, &process_token))
        return false;

    win::ScopedHandle privileged_token;
    if (!createPrivilegedToken(&privileged_token))
        return false;

    if (!ImpersonateLoggedOnUser(privileged_token))
    {
        PLOG(LS_WARNING) << "ImpersonateLoggedOnUser failed";
        return false;
    }

    const DWORD reason = SHTDN_REASON_MAJOR_APPLICATION | SHTDN_REASON_MINOR_MAINTENANCE;

    bool result = !!InitiateSystemShutdownExW(nullptr,
                                              nullptr,
                                              kActionDelayInSeconds,
                                              TRUE, // Force close apps.
                                              TRUE, // Reboot.
                                              reason);
    if (!result)
        PLOG(LS_WARNING) << "InitiateSystemShutdownExW failed";

    BOOL ret = RevertToSelf();
    CHECK(ret);

    return result;
}

// static
bool PowerController::logoff()
{
    if (!WTSLogoffSession(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, FALSE))
    {
        PLOG(LS_WARNING) << "WTSLogoffSession failed";
        return false;
    }

    return true;
}

// static
bool PowerController::lock()
{
    if (!WTSDisconnectSession(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, FALSE))
    {
        PLOG(LS_WARNING) << "WTSDisconnectSession failed";
        return false;
    }

    return true;
}

} // namespace base
