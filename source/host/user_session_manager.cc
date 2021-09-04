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

#include "host/user_session_manager.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/task_runner.h"
#include "base/ipc/ipc_channel.h"
#include "base/files/base_paths.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/win/scoped_impersonator.h"
#include "base/win/scoped_object.h"
#include "base/win/session_enumerator.h"
#include "base/win/session_info.h"
#include "host/client_session.h"
#include "host/user_session.h"
#include "host/user_session_constants.h"

#include <UserEnv.h>

namespace host {

namespace {

const wchar_t kExecutableNameForUi[] = L"aspia_host.exe";

// Name of the default session desktop.
const wchar_t kDefaultDesktopName[] = L"winsta0\\default";

bool createLoggedOnUserToken(DWORD session_id, base::win::ScopedHandle* token_out)
{
    base::win::ScopedHandle user_token;

    if (!WTSQueryUserToken(session_id, user_token.recieve()))
    {
            DWORD error_code = GetLastError();
            if (error_code == ERROR_NO_TOKEN)
            {
                token_out->reset();
                return true;
            }

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

    if (!SetPriorityClass(process_info.hProcess, NORMAL_PRIORITY_CLASS))
    {
        PLOG(LS_WARNING) << "SetPriorityClass failed";
    }

    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
    DestroyEnvironmentBlock(environment);

    return true;
}

} // namespace

UserSessionManager::UserSessionManager(std::shared_ptr<base::TaskRunner> task_runner)
    : task_runner_(std::move(task_runner))
{
    DCHECK(task_runner_);
    router_state_.set_state(proto::internal::RouterState::DISABLED);
}

UserSessionManager::~UserSessionManager() = default;

bool UserSessionManager::start(Delegate* delegate)
{
    DCHECK(delegate);
    DCHECK(!delegate_);

    if (ipc_server_)
        return false;

    delegate_ = delegate;

    ipc_server_ = std::make_unique<base::IpcServer>();

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
    base::win::SessionStatus status, base::SessionId session_id)
{
    // Send an event of each session.
    for (const auto& session : sessions_)
        session->setSessionEvent(status, session_id);

    switch (status)
    {
        case base::win::SessionStatus::CONSOLE_CONNECT:
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

void UserSessionManager::setRouterState(const proto::internal::RouterState& router_state)
{
    router_state_ = router_state;

    // Send an event of each session.
    for (const auto& session : sessions_)
        session->setRouterState(router_state);
}

void UserSessionManager::setHostId(const std::string& session_name, base::HostId host_id)
{
    // Send an event of each session.
    for (const auto& session : sessions_)
    {
        if (session->sessionName() == session_name)
        {
            session->setHostId(host_id);
            delegate_->onUserListChanged();
            return;
        }
    }
}

void UserSessionManager::addNewSession(std::unique_ptr<ClientSession> client_session)
{
    LOG(LS_INFO) << "Adding a new client connection (user: " << client_session->userName() << ")";

    base::SessionId session_id = base::kInvalidSessionId;

    std::string username = client_session->userName();
    if (base::startsWith(username, "#"))
    {
        username.erase(username.begin());

        base::HostId host_id = base::kInvalidHostId;
        if (!base::stringToULong64(username, &host_id))
        {
            LOG(LS_ERROR) << "Failed to convert host id: " << username;
            return;
        }

        for (const auto& session : sessions_)
        {
            if (session->hostId() == host_id)
            {
                session_id = session->sessionId();
                break;
            }
        }

        LOG(LS_INFO) << "Connection by host id";
    }
    else
    {
        LOG(LS_INFO) << "Connecting with a permanent username";
        session_id = base::activeConsoleSessionId();
    }

    if (session_id == base::kInvalidSessionId)
    {
        LOG(LS_ERROR) << "Failed to get session id";
        return;
    }

    for (const auto& session : sessions_)
    {
        if (session->sessionId() == session_id)
        {
            if (session->state() == UserSession::State::DETTACHED)
            {
                base::win::SessionInfo session_info(session_id);
                if (!session_info.isValid())
                {
                    LOG(LS_ERROR) << "Unable to determine session state. Connection aborted";
                    return;
                }

                switch (session_info.connectState())
                {
                    case base::win::SessionInfo::ConnectState::CONNECTED:
                    {
                        LOG(LS_INFO) << "Session exists, but there are no logged in users";
                        session->restart(nullptr);
                    }
                    break;

                    default:
                        LOG(LS_INFO) << "No connected UI. Connection rejected";
                        return;
                }
            }

            session->addNewSession(std::move(client_session));
            break;
        }
    }
}

std::unique_ptr<base::UserList> UserSessionManager::userList() const
{
    std::unique_ptr<base::UserList> user_list = base::UserList::createEmpty();

    for (const auto& session : sessions_)
        user_list->add(session->user());

    return user_list;
}

void UserSessionManager::onNewConnection(std::unique_ptr<base::IpcChannel> channel)
{
    std::filesystem::path reference_path;
    if (!base::BasePaths::currentExecDir(&reference_path))
        return;

    reference_path.append(kExecutableNameForUi);

    if (reference_path != channel->peerFilePath())
    {
        LOG(LS_ERROR) << "An attempt was made to connect from an unknown application";
        return;
    }

    base::SessionId session_id = channel->peerSessionId();
    if (session_id == base::kInvalidSessionId)
        return;

    addUserSession(session_id, std::move(channel));
}

void UserSessionManager::onErrorOccurred()
{
    // Ignore.
}

void UserSessionManager::onUserSessionHostIdRequest(const std::string& session_name)
{
    delegate_->onHostIdRequest(session_name);
}

void UserSessionManager::onUserSessionCredentialsChanged()
{
    delegate_->onUserListChanged();
}

void UserSessionManager::onUserSessionDettached()
{
    delegate_->onUserListChanged();
}

void UserSessionManager::onUserSessionFinished()
{
    for (auto it = sessions_.begin(); it != sessions_.end();)
    {
        if (it->get()->state() == UserSession::State::FINISHED)
        {
            LOG(LS_INFO) << "Finished session found in list. Reset host ID for invalid session";

            // User session ended, host ID is no longer valid.
            delegate_->onResetHostId(it->get()->hostId());

            LOG(LS_INFO) << "Delete session from list...";

            task_runner_->deleteSoon(std::move(*it));
            it = sessions_.erase(it);

            LOG(LS_INFO) << "Session deleted from list";
        }
        else
        {
            ++it;
        }
    }
}

void UserSessionManager::startSessionProcess(base::SessionId session_id)
{
    if (session_id == base::kServiceSessionId)
    {
        LOG(LS_WARNING) << "Invalid session ID";
        return;
    }

    LOG(LS_INFO) << "Starting session process for session ID: " << session_id;

    base::win::ScopedHandle user_token;
    if (!createLoggedOnUserToken(session_id, &user_token))
    {
        LOG(LS_WARNING) << "Failed to get user token";
        return;
    }

    if (!user_token.isValid())
    {
        LOG(LS_INFO) << "Invalid user token. User is not logged in";

        // If there is no user logged in, but the session exists, then add the session without
        // connecting to UI (we cannot start UI if the user is not logged in).
        addUserSession(session_id, nullptr);
        return;
    }

    std::filesystem::path file_path;
    if (!base::BasePaths::currentExecDir(&file_path))
    {
        LOG(LS_WARNING) << "Failed to get current exec directory";
        return;
    }

    file_path.append(kExecutableNameForUi);

    base::CommandLine command_line(file_path);
    command_line.appendSwitch(u"hidden");

    if (!createProcessWithToken(user_token, command_line))
    {
        LOG(LS_WARNING) << "Failed to start process with user token ("
                        << command_line.commandLineString() << ")";
    }
}

void UserSessionManager::addUserSession(
    base::SessionId session_id, std::unique_ptr<base::IpcChannel> channel)
{
    for (const auto& session : sessions_)
    {
        if (session->sessionId() == session_id)
        {
            session->restart(std::move(channel));
            return;
        }
    }

    std::unique_ptr<UserSession> user_session = std::make_unique<UserSession>(
        task_runner_, session_id, std::move(channel));
    user_session->setRouterState(router_state_);

    sessions_.emplace_back(std::move(user_session));
    sessions_.back()->start(this);
}

} // namespace host
