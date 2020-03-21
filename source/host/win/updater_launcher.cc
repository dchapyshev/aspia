//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/command_line.h"
#include "base/logging.h"
#include "base/files/base_paths.h"
#include "base/strings/string_util.h"
#include "base/win/scoped_impersonator.h"
#include "base/win/scoped_object.h"

#include <userenv.h>
#include <wtsapi32.h>

namespace host {

namespace {

// Name of the default session desktop.
const wchar_t kDefaultDesktopName[] = L"winsta0\\default";

bool createLoggedOnUserToken(DWORD session_id, base::win::ScopedHandle* token_out)
{
    base::win::ScopedHandle user_token;
    if (!WTSQueryUserToken(session_id, user_token.recieve()))
    {
        PLOG(LS_WARNING) << "WTSQueryUserToken failed";
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

bool createProcessWithToken(HANDLE token, const base::CommandLine& command_line)
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

    if (!CreateProcessAsUserW(token,
                              nullptr,
                              base::asWritableWide(command_line.commandLineString()),
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

bool launchUpdater(base::SessionId session_id)
{
    if (session_id == base::kInvalidSessionId || session_id == base::kServiceSessionId)
    {
        DLOG(LS_ERROR) << "Invalid session id: " << session_id;
        return false;
    }

    base::win::ScopedHandle user_token;
    if (!createLoggedOnUserToken(session_id, &user_token))
        return false;

    std::filesystem::path file_path;
    if (!base::BasePaths::currentExecDir(&file_path))
        return false;

    file_path.append("aspia_host.exe");

    base::CommandLine command_line(file_path);
    command_line.appendSwitch(u"update");

    return createProcessWithToken(user_token, command_line);
}

} // namespace host
