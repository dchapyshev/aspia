//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "base/stl_util.h"
#include "base/task_runner.h"
#include "base/ipc/ipc_channel.h"
#include "base/files/base_paths.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/string_printf.h"
#include "base/strings/string_split.h"
#include "host/client_session.h"
#include "host/host_ipc_storage.h"
#include "host/user_session.h"

#if defined(OS_WIN)
#include "base/win/scoped_object.h"
#include "base/win/session_enumerator.h"
#include "base/win/session_info.h"

#include <UserEnv.h>
#endif // defined(OS_WIN)

namespace host {

namespace {

#if defined(OS_POSIX)
const char kExecutableNameForUi[] = "aspia_host";
#endif // defined(OS_POSIX)

#if defined(OS_WIN)
const wchar_t kExecutableNameForUi[] = L"aspia_host.exe";

// Name of the default session desktop.
const wchar_t kDefaultDesktopName[] = L"winsta0\\default";

//--------------------------------------------------------------------------------------------------
bool createLoggedOnUserToken(DWORD session_id, base::win::ScopedHandle* token_out)
{
    if (!WTSQueryUserToken(session_id, token_out->recieve()))
    {
        DWORD error_code = GetLastError();
        if (error_code == ERROR_NO_TOKEN)
        {
            token_out->reset();
            return true;
        }

        PLOG(LS_ERROR) << "WTSQueryUserToken failed";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool createProcessWithToken(HANDLE token, const base::CommandLine& command_line)
{
    STARTUPINFOW startup_info;
    memset(&startup_info, 0, sizeof(startup_info));

    startup_info.cb = sizeof(startup_info);
    startup_info.lpDesktop = const_cast<wchar_t*>(kDefaultDesktopName);

    void* environment = nullptr;

    if (!CreateEnvironmentBlock(&environment, token, FALSE))
    {
        PLOG(LS_ERROR) << "CreateEnvironmentBlock failed";
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
        PLOG(LS_ERROR) << "CreateProcessAsUserW failed";
        if (!DestroyEnvironmentBlock(environment))
        {
            PLOG(LS_ERROR) << "DestroyEnvironmentBlock failed";
        }
        return false;
    }

    if (!SetPriorityClass(process_info.hProcess, NORMAL_PRIORITY_CLASS))
    {
        PLOG(LS_ERROR) << "SetPriorityClass failed";
    }

    if (!CloseHandle(process_info.hThread))
    {
        PLOG(LS_ERROR) << "CloseHandle failed";
    }

    if (!CloseHandle(process_info.hProcess))
    {
        PLOG(LS_ERROR) << "CloseHandle failed";
    }

    if (!DestroyEnvironmentBlock(environment))
    {
        PLOG(LS_ERROR) << "DestroyEnvironmentBlock failed";
    }

    return true;
}
#endif // defined(OS_WIN)

} // namespace

//--------------------------------------------------------------------------------------------------
UserSessionManager::UserSessionManager(std::shared_ptr<base::TaskRunner> task_runner)
    : task_runner_(std::move(task_runner))
{
    LOG(LS_INFO) << "Ctor";
    DCHECK(task_runner_);

    scoped_task_runner_ = std::make_unique<base::ScopedTaskRunner>(task_runner_);
    router_state_.set_state(proto::internal::RouterState::DISABLED);
}

//--------------------------------------------------------------------------------------------------
UserSessionManager::~UserSessionManager()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
bool UserSessionManager::start(Delegate* delegate)
{
    DCHECK(delegate);
    DCHECK(!delegate_);

    LOG(LS_INFO) << "Starting user session manager";

    if (ipc_server_)
    {
        LOG(LS_ERROR) << "IPC server already exists";
        return false;
    }

    delegate_ = delegate;

    ipc_server_ = std::make_unique<base::IpcServer>();

    std::u16string ipc_channel_for_ui = base::IpcServer::createUniqueId();
    HostIpcStorage ipc_storage;
    ipc_storage.setChannelIdForUI(ipc_channel_for_ui);

    // Start the server which will accept incoming connections from UI processes in user sessions.
    if (!ipc_server_->start(ipc_channel_for_ui, this))
    {
        LOG(LS_ERROR) << "Failed to start IPC server for UI";
        return false;
    }

#if defined(OS_WIN)
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
                             << "Session process will not be started (name: '" << name << "' state: "
                             << base::win::SessionEnumerator::stateToString(session.state()) << ")";
                continue;
            }
        }

        LOG(LS_INFO) << "Starting process for session id: " << session_id << " (name: '" << name
                     << "' state: " << base::win::SessionEnumerator::stateToString(session.state())
                     << ")";

        // Start UI process in user session.
        startSessionProcess(FROM_HERE, session.sessionId());
    }
#else
    startSessionProcess(FROM_HERE, 0);
#endif

    LOG(LS_INFO) << "User session manager is started";
    return true;
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onUserSessionEvent(
    base::win::SessionStatus status, base::SessionId session_id)
{
    // Send an event of each session.
    for (const auto& session : sessions_)
        session->onUserSessionEvent(status, session_id);

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

        case base::win::SessionStatus::SESSION_UNLOCK:
        {
            for (const auto& session : sessions_)
            {
                if (session->sessionId() == session_id)
                {
                    if (!session->isConnectedToUi())
                    {
                        // Start UI process in user session.
                        startSessionProcess(FROM_HERE, session_id);
                    }
                    break;
                }
            }
        }
        break;

        default:
        {
            // Ignore other events.
        }
        break;
    }
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onRouterStateChanged(const proto::internal::RouterState& router_state)
{
    LOG(LS_INFO) << "New router state";
    router_state_ = router_state;

    // Send an event of each session.
    for (const auto& session : sessions_)
        session->onRouterStateChanged(router_state);
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onHostIdChanged(const std::string& session_name, base::HostId host_id)
{
    LOG(LS_INFO) << "Set host ID for session '" << session_name << "': " << host_id;

    // Send an event of each session.
    for (const auto& session : sessions_)
    {
        std::optional<std::string> name = session->sessionName();
        if (name.has_value() && name == session_name)
        {
            LOG(LS_INFO) << "Session '" << session_name << "' found. Host ID assigned";

            session->onHostIdChanged(host_id);
            delegate_->onUserListChanged();
            return;
        }
    }

    LOG(LS_ERROR) << "Session '" << session_name << "' NOT found";
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onSettingsChanged()
{
    // Send an event of each session.
    for (const auto& session : sessions_)
        session->onSettingsChanged();
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onClientSession(std::unique_ptr<ClientSession> client_session)
{
    LOG(LS_INFO) << "Adding a new client connection (user: '" << client_session->userName()
                 << "', host_id: '" << client_session->hostId() << "')";

    base::SessionId session_id = base::kInvalidSessionId;

    std::string username = client_session->userName();
    if (base::startsWith(username, "#"))
    {
        LOG(LS_INFO) << "Connection with one time password";

        username.erase(username.begin());

        base::HostId host_id = base::kInvalidHostId;
        if (!base::stringToULong64(username, &host_id))
        {
            LOG(LS_ERROR) << "Failed to convert host id: " << username;
            return;
        }

        LOG(LS_INFO) << "Find session ID by host ID...";

        for (const auto& session : sessions_)
        {
            if (session->hostId() == host_id)
            {
                session_id = session->sessionId();

                LOG(LS_INFO) << "Session with ID '" << session_id << "' found";
                break;
            }
        }

        LOG(LS_INFO) << "Connection by host id: " << host_id;
    }
    else
    {
        LOG(LS_INFO) << "Connecting with a permanent username";

        base::HostId host_id = client_session->hostId();
        if (host_id == base::kInvalidHostId)
        {
            LOG(LS_INFO) << "Using CONSOLE session id";

#if defined(OS_WIN)
            session_id = base::activeConsoleSessionId();
#else
            session_id = 0;
#endif
        }
        else
        {
            LOG(LS_INFO) << "Find session ID by host ID...";

            for (const auto& session : sessions_)
            {
                if (session->hostId() == host_id)
                {
                    session_id = session->sessionId();

                    LOG(LS_INFO) << "Session with ID '" << session_id << "' found";
                    break;
                }
            }
        }
    }

    if (session_id == base::kInvalidSessionId)
    {
        LOG(LS_ERROR) << "Failed to get session id";
        return;
    }

    bool user_session_found = false;

    for (const auto& session : sessions_)
    {
        if (session->sessionId() == session_id)
        {
            if (session->state() == UserSession::State::DETTACHED)
            {
#if defined(OS_WIN)
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
#else
                session->restart(nullptr);
#endif
            }

            session->onClientSession(std::move(client_session));
            user_session_found = true;
            break;
        }
    }

    if (!user_session_found)
    {
        LOG(LS_ERROR) << "User session with id " << session_id << " not found";
    }
}

//--------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onNewConnection(std::unique_ptr<base::IpcChannel> channel)
{
    LOG(LS_INFO) << "New IPC connection";

#if defined(OS_WIN)
    std::filesystem::path reference_path;
    if (!base::BasePaths::currentExecDir(&reference_path))
    {
        LOG(LS_ERROR) << "currentExecDir failed";
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
        LOG(LS_ERROR) << "Invalid session id";
        return;
    }

    addUserSession(session_id, std::move(channel));
#else
    addUserSession(0, std::move(channel));
#endif
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onErrorOccurred()
{
    // Ignore.
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onUserSessionHostIdRequest(const std::string& session_name)
{
    delegate_->onHostIdRequest(session_name);
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onUserSessionCredentialsChanged()
{
    delegate_->onUserListChanged();
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onUserSessionDettached()
{
    delegate_->onUserListChanged();
}

//--------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------------------
void UserSessionManager::startSessionProcess(
    const base::Location& location, base::SessionId session_id)
{
    LOG(LS_INFO) << "Starting UI process (sid: " << session_id
                 << " from: " << location.toString() << ")";

#if defined(OS_WIN)
    if (session_id == base::kInvalidSessionId)
    {
        LOG(LS_ERROR) << "An attempt was detected to start a process in a INVALID session";
        return;
    }

    if (session_id == base::kServiceSessionId)
    {
        LOG(LS_ERROR) << "An attempt was detected to start a process in a SERVICES session";
        return;
    }

    base::win::ScopedHandle user_token;
    if (!createLoggedOnUserToken(session_id, &user_token))
    {
        LOG(LS_ERROR) << "Failed to get user token (sid: " << session_id << ")";
        return;
    }

    if (!user_token.isValid())
    {
        LOG(LS_INFO) << "User is not logged in (sid: " << session_id << ")";

        // If there is no user logged in, but the session exists, then add the session without
        // connecting to UI (we cannot start UI if the user is not logged in).
        addUserSession(session_id, nullptr);
        return;
    }

    base::win::SessionInfo session_info(session_id);
    if (!session_info.isValid())
    {
        LOG(LS_ERROR) << "Unable to get session info (sid: " << session_id << ")";
        return;
    }

    if (session_info.isUserLocked())
    {
        LOG(LS_INFO) << "Session has LOCKED user (sid: " << session_id << ")";

        // If the user session is locked, then we do not start the UI process. The process will be
        // started later when the user session is unlocked.
        addUserSession(session_id, nullptr);
        return;
    }
    else
    {
        LOG(LS_INFO) << "Session has UNLOCKED user (sid: " << session_id << ")";
    }

    std::filesystem::path file_path;
    if (!base::BasePaths::currentExecDir(&file_path))
    {
        LOG(LS_ERROR) << "Failed to get current exec directory (sid: " << session_id << ")";
        return;
    }

    file_path.append(kExecutableNameForUi);

    base::CommandLine command_line(file_path);
    command_line.appendSwitch(u"hidden");

    if (!createProcessWithToken(user_token, command_line))
    {
        LOG(LS_ERROR) << "Failed to start process with user token (sid: " << session_id
                        << " cmd: " << command_line.commandLineString() << ")";
    }
#elif defined(OS_LINUX)
    std::error_code ignored_error;
    std::filesystem::directory_iterator it("/usr/share/xsessions/", ignored_error);
    if (it == std::filesystem::end(it))
    {
        LOG(LS_ERROR) << "No X11 sessions";
        return;
    }

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("who", "r"), pclose);
    if (!pipe)
    {
        LOG(LS_ERROR) << "Unable to open pipe";
        return;
    }

    std::array<char, 512> buffer;
    while (fgets(buffer.data(), buffer.size(), pipe.get()))
    {
        std::u16string line = base::toLower(base::utf16FromLocal8Bit(buffer.data()));

        if (base::contains(line, u":0") || base::contains(line, u"tty2"))
        {
            std::vector<std::u16string_view> splitted = base::splitStringView(
                line, u" ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

            if (!splitted.empty())
            {
                std::string user_name = base::local8BitFromUtf16(splitted.front());

                std::filesystem::path file_path;
                if (!base::BasePaths::currentExecDir(&file_path))
                {
                    LOG(LS_ERROR) << "Failed to get current exec directory (sid: " << session_id << ")";
                    return;
                }

                file_path.append(kExecutableNameForUi);

                std::string command_line =
                    base::stringPrintf("sudo DISPLAY=':0' FONTCONFIG_PATH=/etc/fonts "
                                       "DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/$(id -u %s)/bus "
                                       "-u %s %s --hidden &",
                                       user_name.c_str(),
                                       user_name.c_str(),
                                       file_path.c_str());

                int ret = system(command_line.c_str());
                LOG(LS_INFO) << "system result: " << ret;
            }
        }
    }
#else
    NOTIMPLEMENTED();
#endif
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::addUserSession(
    base::SessionId session_id, std::unique_ptr<base::IpcChannel> channel)
{
    LOG(LS_INFO) << "Add user session: " << session_id;

    for (const auto& session : sessions_)
    {
        if (session->sessionId() == session_id)
        {
            LOG(LS_INFO) << "Restart user session: " << session_id;
            session->restart(std::move(channel));
            return;
        }
    }

    std::unique_ptr<UserSession> user_session = std::make_unique<UserSession>(
        task_runner_, session_id, std::move(channel), this);

    LOG(LS_INFO) << "Start user session: " << session_id;
    sessions_.emplace_back(std::move(user_session));
    sessions_.back()->start(router_state_);
}

} // namespace host
