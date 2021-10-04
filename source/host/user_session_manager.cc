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
#include "base/location.h"
#include "base/logging.h"
#include "base/scoped_task_runner.h"
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
    LOG(LS_INFO) << "UserSessionManager Ctor";
    DCHECK(task_runner_);

    scoped_task_runner_ = std::make_unique<base::ScopedTaskRunner>(task_runner_);
    router_state_.set_state(proto::internal::RouterState::DISABLED);
}

UserSessionManager::~UserSessionManager()
{
    LOG(LS_INFO) << "UserSessionManager Dtor";
}

bool UserSessionManager::start(Delegate* delegate)
{
    DCHECK(delegate);
    DCHECK(!delegate_);

    LOG(LS_INFO) << "Starting user session manager";

    if (ipc_server_)
    {
        LOG(LS_WARNING) << "IPC server already exists";
        return false;
    }

    delegate_ = delegate;

    ipc_server_ = std::make_unique<base::IpcServer>();

    // Start the server which will accept incoming connections from UI processes in user sessions.
    if (!ipc_server_->start(kIpcChannelIdForUI, this))
    {
        LOG(LS_WARNING) << "Failed to start IPC servcer for UI";
        return false;
    }

    for (base::win::SessionEnumerator session; !session.isAtEnd(); session.advance())
    {
        base::SessionId session_id = session.sessionId();

        // Skip invalid session ID.
        if (session_id == base::kInvalidSessionId)
            continue;

        // Never run processes in a service session.
        if (session_id == base::kServiceSessionId)
            continue;

        std::u16string name = session.sessionName16();
        if (base::compareCaseInsensitive(name, u"console") != 0)
        {
            // RDP session detected.
            if (session.state() != WTSActive)
            {
                LOG(LS_INFO) << "RDP session with ID " << session_id << " not in active state. "
                             << "Session process will not be started";
                continue;
            }
        }

        // Start UI process in user session.
        startSessionProcess(FROM_HERE, session.sessionId());
    }

    LOG(LS_INFO) << "User session manager is started";
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
        case base::win::SessionStatus::REMOTE_CONNECT:
        case base::win::SessionStatus::SESSION_LOGON:
        {
            // Start UI process in user session.
            startSessionProcess(FROM_HERE, session_id);
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
    LOG(LS_INFO) << "New router state";
    router_state_ = router_state;

    // Send an event of each session.
    for (const auto& session : sessions_)
        session->setRouterState(router_state);
}

void UserSessionManager::setHostId(const std::string& session_name, base::HostId host_id)
{
    LOG(LS_INFO) << "Set host ID for session '" << session_name << "': " << host_id;

    // Send an event of each session.
    for (const auto& session : sessions_)
    {
        std::optional<std::string> name = session->sessionName();
        if (name.has_value() && name == session_name)
        {
            LOG(LS_INFO) << "Session '" << session_name << "' found. Host ID assigned";

            session->setHostId(host_id);
            delegate_->onUserListChanged();
            return;
        }
    }

    LOG(LS_WARNING) << "Session '" << session_name << "' NOT found";
}

void UserSessionManager::onSettingsChanged()
{
    // Send an event of each session.
    for (const auto& session : sessions_)
        session->onSettingsChanged();
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
    {
        base::User user = session->user();
        if (user.isValid())
            user_list->add(user);
    }

    return user_list;
}

void UserSessionManager::onNewConnection(std::unique_ptr<base::IpcChannel> channel)
{
    LOG(LS_INFO) << "New IPC connection";

    std::filesystem::path reference_path;
    if (!base::BasePaths::currentExecDir(&reference_path))
    {
        LOG(LS_WARNING) << "currentExecDir failed";
        return;
    }

    reference_path.append(kExecutableNameForUi);

    if (reference_path != channel->peerFilePath())
    {
        LOG(LS_ERROR) << "An attempt was made to connect from an unknown application";
        return;
    }

    base::SessionId session_id = channel->peerSessionId();
    if (session_id == base::kInvalidSessionId)
    {
        LOG(LS_WARNING) << "Invalid session id";
        return;
    }

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
    scoped_task_runner_->postTask([this]()
    {
        for (auto it = sessions_.begin(); it != sessions_.end();)
        {
            if (it->get()->state() == UserSession::State::FINISHED)
            {
                UserSession* session = it->get();

                LOG(LS_INFO) << "Finished session " << session->sessionId()
                             << " found in list. Reset host ID "
                             << session->hostId() << " for invalid session";

                // User session ended, host ID is no longer valid.
                delegate_->onResetHostId(session->hostId());

                task_runner_->deleteSoon(std::move(*it));
                it = sessions_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    });
}

void UserSessionManager::startSessionProcess(
    const base::Location& location, base::SessionId session_id)
{
    LOG(LS_INFO) << "Starting UI process for session ID: " << session_id
                 << " (from: " << location.toString() << ")";

    if (session_id == base::kServiceSessionId)
    {
        LOG(LS_WARNING) << "Invalid session ID";
        return;
    }

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
    LOG(LS_INFO) << "Add user session: " << session_id;

    for (const auto& session : sessions_)
    {
        if (session->sessionId() == session_id)
        {
            LOG(LS_INFO) << "Restart user session";
            session->restart(std::move(channel));
            return;
        }
    }

    std::unique_ptr<UserSession> user_session = std::make_unique<UserSession>(
        task_runner_, session_id, std::move(channel), this);

    LOG(LS_INFO) << "Start user session";
    sessions_.emplace_back(std::move(user_session));
    sessions_.back()->start(router_state_);
}

} // namespace host
