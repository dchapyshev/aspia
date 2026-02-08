//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include <QDir>

#include <UserEnv.h>
#include <WtsApi32.h>

#include "base/logging.h"
#include "base/win/scoped_object.h"

namespace host {

namespace {

// Name of the default session desktop.
const wchar_t kDefaultDesktopName[] = L"winsta0\\default";

//--------------------------------------------------------------------------------------------------
bool createLoggedOnUserToken(DWORD session_id, base::ScopedHandle* token_out)
{
    base::ScopedHandle user_token;
    if (!WTSQueryUserToken(session_id, user_token.recieve()))
    {
        PLOG(ERROR) << "WTSQueryUserToken failed";
        return false;
    }

    TOKEN_ELEVATION_TYPE elevation_type;
    DWORD returned_length;

    if (!GetTokenInformation(user_token,
                             TokenElevationType,
                             &elevation_type,
                             sizeof(elevation_type),
                             &returned_length))
    {
        PLOG(ERROR) << "GetTokenInformation failed";
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
                PLOG(ERROR) << "GetTokenInformation failed";
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

//--------------------------------------------------------------------------------------------------
bool createProcessWithToken(HANDLE token, const QString& command_line)
{
    STARTUPINFOW startup_info;
    memset(&startup_info, 0, sizeof(startup_info));

    startup_info.cb = sizeof(startup_info);
    startup_info.lpDesktop = const_cast<wchar_t*>(kDefaultDesktopName);

    void* environment = nullptr;

    if (!CreateEnvironmentBlock(&environment, token, FALSE))
    {
        PLOG(ERROR) << "CreateEnvironmentBlock failed";
        return false;
    }

    PROCESS_INFORMATION process_info;
    memset(&process_info, 0, sizeof(process_info));

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
        PLOG(ERROR) << "CreateProcessAsUserW failed";
        DestroyEnvironmentBlock(environment);
        return false;
    }

    base::ScopedHandle thread_deleter(process_info.hThread);
    base::ScopedHandle process_deleter(process_info.hProcess);

    if (!DestroyEnvironmentBlock(environment))
    {
        PLOG(ERROR) << "DestroyEnvironmentBlock failed";
    }

    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
bool launchUpdater(base::SessionId session_id)
{
    if (session_id == base::kInvalidSessionId || session_id == base::kServiceSessionId)
    {
        LOG(ERROR) << "Invalid session id: " << session_id;
        return false;
    }

    base::ScopedHandle user_token;
    if (!createLoggedOnUserToken(session_id, &user_token))
    {
        LOG(ERROR) << "createLoggedOnUserToken failed";
        return false;
    }

    QString file_dir = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
    QString command_line = file_dir + "\\smartcafe_host.exe --update";

    return createProcessWithToken(user_token, command_line);
}

} // namespace host
