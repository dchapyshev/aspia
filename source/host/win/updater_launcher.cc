//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/win/updater_launcher.h"

#include <QCoreApplication>

#include <userenv.h>
#include <wtsapi32.h>

#include "base/win/process.h"
#include "base/win/process_util.h"
#include "base/win/scoped_impersonator.h"
#include "base/logging.h"

namespace host {

namespace {

// Name of the default session desktop.
const wchar_t kDefaultDesktopName[] = L"winsta0\\default";

bool createLoggedOnUserToken(DWORD session_id, base::win::ScopedHandle* token_out)
{
    base::win::ScopedHandle privileged_token;

    if (!createPrivilegedToken(&privileged_token))
        return false;

    base::win::ScopedImpersonator impersonator;
    if (!impersonator.loggedOnUser(privileged_token))
        return false;

    base::win::ScopedHandle user_token;
    if (!WTSQueryUserToken(session_id, user_token.recieve()))
    {
        PLOG(LS_WARNING) << "WTSQueryUserToken failed";
        return false;
    }

    impersonator.revertToSelf();

    TOKEN_ELEVATION_TYPE elevation_type;
    DWORD returned_length;

    if (!GetTokenInformation(user_token,
                             TokenElevationType,
                             &elevation_type,
                             sizeof(elevation_type),
                             &returned_length))
    {
        PLOG(LS_WARNING) << "GetTokenInformation failed";
        return false;
    }

    switch (elevation_type)
    {
        // The token is a limited token.
        case TokenElevationTypeLimited:
        {
            TOKEN_LINKED_TOKEN linked_token_info;

            // Get the unfiltered token for a silent UAC bypass.
            if (!GetTokenInformation(user_token,
                                     TokenLinkedToken,
                                     &linked_token_info,
                                     sizeof(linked_token_info),
                                     &returned_length))
            {
                PLOG(LS_WARNING) << "GetTokenInformation failed";
                return false;
            }

            // Attach linked token.
            token_out->reset(linked_token_info.LinkedToken);
        }
        break;

        case TokenElevationTypeDefault: // The token does not have a linked token.
        case TokenElevationTypeFull:    // The token is an elevated token.
        default:
            token_out->reset(user_token.release());
            break;
    }

    return true;
}

bool createProcessWithToken(HANDLE token, const QString& program, const QStringList& arguments)
{
    STARTUPINFOW startup_info;
    memset(&startup_info, 0, sizeof(startup_info));

    startup_info.cb = sizeof(startup_info);
    startup_info.lpDesktop = const_cast<wchar_t*>(kDefaultDesktopName);

    void* environment = nullptr;

    if (!CreateEnvironmentBlock(&environment, token, FALSE))
    {
        PLOG(LS_WARNING) << "CreateEnvironmentBlock failed";
        return false;
    }

    PROCESS_INFORMATION process_info;
    memset(&process_info, 0, sizeof(process_info));

    QString command_line = base::win::Process::createCommandLine(program, arguments);

    if (!CreateProcessAsUserW(token,
                              nullptr,
                              const_cast<wchar_t*>(qUtf16Printable(command_line)),
                              nullptr,
                              nullptr,
                              FALSE,
                              CREATE_UNICODE_ENVIRONMENT,
                              environment,
                              nullptr,
                              &startup_info,
                              &process_info))
    {
        PLOG(LS_WARNING) << "CreateProcessAsUserW failed";
        DestroyEnvironmentBlock(environment);
        return false;
    }

    base::win::ScopedHandle thread_deleter(process_info.hThread);
    base::win::ScopedHandle process_deleter(process_info.hProcess);

    DestroyEnvironmentBlock(environment);
    return true;
}

} // namespace

bool launchUpdater()
{
    const DWORD kInvalidSessionId = 0xFFFFFFFF;

    DWORD session_id = WTSGetActiveConsoleSessionId();
    if (session_id == kInvalidSessionId)
    {
        PLOG(LS_WARNING) << "WTSGetActiveConsoleSessionId failed";
        return false;
    }

    base::win::ScopedHandle user_token;
    if (!createLoggedOnUserToken(session_id, &user_token))
        return false;

    QString program = QCoreApplication::applicationDirPath() + QLatin1String("/aspia_host.exe");

    QStringList arguments;
    arguments << QLatin1String("--update");

    return createProcessWithToken(user_token, program, arguments);
}

} // namespace host
