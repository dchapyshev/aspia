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

#include "host/user_session_manager.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/files/base_paths.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/win/scoped_impersonator.h"
#include "base/win/scoped_object.h"
#include "base/win/session_enumerator.h"
#include "host/client_session.h"
#include "host/user_session.h"
#include "host/user_session_constants.h"
#include "ipc/ipc_channel.h"

#include <userenv.h>

namespace host {

namespace {

const wchar_t kExecutableNameForUi[] = L"aspia_host.exe";

// Name of the default session desktop.
const wchar_t kDefaultDesktopName[] = L"winsta0\\default";

bool copyProcessToken(DWORD desired_access, base::win::ScopedHandle* token_out)
{
    base::win::ScopedHandle process_token;

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

bool createPrivilegedToken(base::win::ScopedHandle* token_out)
{
    base::win::ScopedHandle privileged_token;
    const DWORD desired_access = TOKEN_ADJUST_PRIVILEGES | TOKEN_IMPERSONATE |
        TOKEN_DUPLICATE | TOKEN_QUERY;

    if (!copyProcessToken(desired_access, &privileged_token))
        return false;

    // Get the LUID for the SE_TCB_NAME privilege.
    TOKEN_PRIVILEGES state;
    state.PrivilegeCount = 1;
    state.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!LookupPrivilegeValueW(nullptr, SE_TCB_NAME, &state.Privileges[0].Luid))
    {
        PLOG(LS_WARNING) << "LookupPrivilegeValueW failed";
        return false;
    }

    // Enable the SE_TCB_NAME privilege.
    if (!AdjustTokenPrivileges(privileged_token, FALSE, &state, 0, nullptr, nullptr))
    {
        PLOG(LS_WARNING) << "AdjustTokenPrivileges failed";
        return false;
    }

    token_out->reset(privileged_token.release());
    return true;
}

bool createLoggedOnUserToken(DWORD session_id, base::win::ScopedHandle* token_out)
{
    base::win::ScopedHandle privileged_token;

    if (!createPrivilegedToken(&privileged_token))
        return false;

    base::win::ScopedHandle user_token;

    {
        base::win::ScopedImpersonator impersonator;

        if (!impersonator.loggedOnUser(privileged_token))
            return false;

        if (!WTSQueryUserToken(session_id, user_token.recieve()))
        {
            PLOG(LS_WARNING) << "WTSQueryUserToken failed";
            return false;
        }
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

    DWORD ui_access = 1;
    if (!SetTokenInformation(token_out->get(), TokenUIAccess, &ui_access, sizeof(ui_access)))
    {
        PLOG(LS_WARNING) << "SetTokenInformation failed";
        return false;
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
                              const_cast<wchar_t*>(reinterpret_cast<const wchar_t*>(
                                  command_line.commandLineString().c_str())),
                              nullptr,
                              nullptr,
                              FALSE,
                              CREATE_UNICODE_ENVIRONMENT | HIGH_PRIORITY_CLASS,
                              environment,
                              nullptr,
                              &startup_info,
                              &process_info))
    {
        PLOG(LS_WARNING) << "CreateProcessAsUserW failed";
        DestroyEnvironmentBlock(environment);
        return false;
    }

    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
    DestroyEnvironmentBlock(environment);

    return true;
}

} // namespace

UserSessionManager::UserSessionManager() = default;

UserSessionManager::~UserSessionManager() = default;

bool UserSessionManager::start(Delegate* delegate)
{
    DCHECK(delegate);
    DCHECK(!delegate_);

    if (ipc_server_)
        return false;

    delegate_ = delegate;

    ipc_server_ = std::make_unique<ipc::Server>();

    // Start the server which will accept incoming connections from UI processes in user sessions.
    if (!ipc_server_->start(kIpcChannelIdForUI, this))
        return false;

    for (base::win::SessionEnumerator session; !session.isAtEnd(); session.advance())
    {
        // Start UI process in user session.
        startSessionProcess(session.sessionId());
    }

    return true;
}

void UserSessionManager::setSessionEvent(
    base::win::SessionStatus status, base::win::SessionId session_id)
{
    switch (status)
    {
        case base::win::SessionStatus::SESSION_LOGON:
        {
            // Start UI process in user session.
            startSessionProcess(session_id);
        }
        break;

        default:
        {
            // Ignore other events.
        }
        break;
    }
}

void UserSessionManager::addNewSession(std::unique_ptr<ClientSession> client_session)
{
    base::win::SessionId session_id;

    std::u16string username = client_session->userName();
    if (base::startsWith(username, u"#"))
    {
        username.erase(username.begin());

        if (!base::stringToULong(username, &session_id))
            return;
    }
    else
    {
        session_id = base::win::activeConsoleSessionId();
    }

    for (const auto& session : sessions_)
    {
        if (session->sessionId() == session_id)
        {
            session->addNewSession(std::move(client_session));
            break;
        }
    }
}

UserList UserSessionManager::userList() const
{
    UserList user_list;

    for (const auto& session : sessions_)
        user_list.add(session->user());

    return user_list;
}

void UserSessionManager::onNewConnection(std::unique_ptr<ipc::Channel> channel)
{
    sessions_.emplace_back(std::make_unique<UserSession>(std::move(channel)));
    sessions_.back()->start(this);
}

void UserSessionManager::onErrorOccurred()
{
    // TODO
}

void UserSessionManager::startSessionProcess(base::win::SessionId session_id)
{
    if (session_id == base::win::kServiceSessionId)
        return;

    base::win::ScopedHandle user_token;
    if (!createLoggedOnUserToken(session_id, &user_token))
        return;

    std::filesystem::path file_path;
    if (!base::BasePaths::currentExecDir(&file_path))
        return;

    file_path.append(kExecutableNameForUi);

    base::CommandLine command_line(file_path);
    command_line.appendSwitch(u"hidden");

    createProcessWithToken(user_token, command_line);
}

} // namespace host
