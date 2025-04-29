//
// Aspia Project
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

#include "host/user_session_manager.h"

#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/scoped_task_runner.h"
#include "base/task_runner.h"
#include "base/ipc/ipc_channel.h"
#include "base/files/base_paths.h"
#include "base/strings/string_util.h"
#include "host/client_session.h"
#include "host/host_ipc_storage.h"
#include "host/user_session.h"

#if defined(OS_WIN)
#include "base/win/desktop.h"
#include "base/win/scoped_object.h"
#include "base/win/session_enumerator.h"
#include "base/win/session_info.h"
#include "base/win/window_station.h"

#include <UserEnv.h>
#endif // defined(OS_WIN)

#include <fmt/format.h>

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

    QString ipc_channel_for_ui = base::IpcServer::createUniqueId();
    HostIpcStorage ipc_storage;
    ipc_storage.setChannelIdForUI(ipc_channel_for_ui);

    LOG(LS_INFO) << "Start IPC server for UI (channel_id=" << ipc_channel_for_ui << ")";

    // Start the server which will accept incoming connections from UI processes in user sessions.
    if (!ipc_server_->start(ipc_channel_for_ui, this))
    {
        LOG(LS_ERROR) << "Failed to start IPC server for UI (channel_id=" << ipc_channel_for_ui << ")";
        return false;
    }
    else
    {
        LOG(LS_INFO) << "IPC channel for UI started";
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
            LOG(LS_INFO) << "RDP session detected";

            if (session.state() != WTSActive)
            {
                LOG(LS_INFO) << "RDP session with ID " << session_id << " not in active state. "
                             << "Session process will not be started (name='" << name << "' state="
                             << base::win::SessionEnumerator::stateToString(session.state()) << ")";
                continue;
            }
            else
            {
                LOG(LS_INFO) << "RDP session in active state";
            }
        }
        else
        {
            LOG(LS_INFO) << "CONSOLE session detected";
        }

        LOG(LS_INFO) << "Starting process for session id: " << session_id << " (name='" << name
                     << "' state=" << base::win::SessionEnumerator::stateToString(session.state())
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
    std::string status_str;
#if defined(OS_WIN)
    status_str = base::win::sessionStatusToString(status);
#else
    status_str = base::numberToString(static_cast<int>(status));
#endif

    LOG(LS_INFO) << "User session event (status=" << status_str << " session_id=" << session_id << ")";

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
                        LOG(LS_INFO) << "Starting session process for session: " << session_id;
                        startSessionProcess(FROM_HERE, session_id);
                    }
                    else
                    {
                        LOG(LS_INFO) << "Session proccess already connected for session: "
                                     << session_id;
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
    LOG(LS_INFO) << "Router state changed (state=" << router_state.state() << ")";
    router_state_ = router_state;

    // Send an event of each session.
    for (const auto& session : sessions_)
        session->onRouterStateChanged(router_state);
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onHostIdChanged(const QString& session_name, base::HostId host_id)
{
    LOG(LS_INFO) << "Set host ID for session '" << session_name << "': " << host_id;

    // Send an event of each session.
    for (const auto& session : sessions_)
    {
        std::optional<QString> name = session->sessionName();
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
    LOG(LS_INFO) << "Settings changed";

    // Send an event of each session.
    for (const auto& session : sessions_)
        session->onSettingsChanged();
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onClientSession(std::unique_ptr<ClientSession> client_session)
{
    LOG(LS_INFO) << "Adding a new client connection (user='" << client_session->userName()
                 << "' host_id='" << client_session->hostId() << "')";

    base::SessionId session_id = base::kInvalidSessionId;

    QString username = client_session->userName();
    if (username.startsWith('#'))
    {
        LOG(LS_INFO) << "Connection with one-time password";

        username.remove('#');

        base::HostId host_id = base::stringToHostId(username);
        if (host_id == base::kInvalidHostId)
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

                base::win::SessionInfo::ConnectState connect_state = session_info.connectState();

                switch (connect_state)
                {
                    case base::win::SessionInfo::ConnectState::CONNECTED:
                    {
                        LOG(LS_INFO) << "Session exists, but there are no logged in users";
                        session->restart(nullptr);
                    }
                    break;

                    default:
                        LOG(LS_INFO) << "No connected UI. Connection rejected (connect_state="
                                     << base::win::SessionInfo::connectStateToString(connect_state)
                                     << ")";
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
        {
            LOG(LS_INFO) << "User '" << user.name << "' added to user list";
            user_list->add(user);
        }
        else
        {
            LOG(LS_INFO) << "User is invalid and not added to user list";
        }
    }

    return user_list;
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onNewConnection()
{
    LOG(LS_INFO) << "New IPC connection";

    if (!ipc_server_)
    {
        LOG(LS_ERROR) << "No IPC server instance!";
        return;
    }

    if (!ipc_server_->hasPendingConnections())
    {
        LOG(LS_ERROR) << "No pending connections in IPC server";
        return;
    }

    base::IpcChannel* channel = ipc_server_->nextPendingConnection();

#if defined(OS_WIN)
    base::SessionId session_id = channel->peerSessionId();
    if (session_id == base::kInvalidSessionId)
    {
        LOG(LS_ERROR) << "Invalid session id";
        return;
    }

    addUserSession(FROM_HERE, session_id, channel);
#else
    addUserSession(FROM_HERE, 0, channel);
#endif
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onErrorOccurred()
{
    // Ignore.
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onUserSessionHostIdRequest(const QString& session_name)
{
    LOG(LS_INFO) << "User session host id request for session name: " << session_name;
    delegate_->onHostIdRequest(session_name);
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onUserSessionCredentialsChanged()
{
    LOG(LS_INFO) << "User session credentials changed";
    delegate_->onUserListChanged();
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onUserSessionDettached()
{
    LOG(LS_INFO) << "User session dettached";
    delegate_->onUserListChanged();
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onUserSessionFinished()
{
    LOG(LS_INFO) << "User session finished";

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
    LOG(LS_INFO) << "Starting UI process (sid=" << session_id
                 << " from=" << location.toString() << ")";

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

    LOG(LS_INFO) << "Starting user session";
    LOG(LS_INFO) << "#####################################################";

    LOG(LS_INFO) << "# Active console session id: " << WTSGetActiveConsoleSessionId();

    for (const auto& window_station_name : base::WindowStation::windowStationList())
    {
        std::wstring desktops;

        base::WindowStation window_station = base::WindowStation::open(window_station_name.data());
        if (window_station.isValid())
        {
            std::vector<std::wstring> list = base::Desktop::desktopList(window_station.get());

            for (size_t i = 0; i < list.size(); ++i)
            {
                desktops += list[i];
                if ((i + 1) != list.size())
                        desktops += L", ";
            }
        }

        LOG(LS_INFO) << "# " << window_station_name << " (desktops: " << desktops << ")";
    }

    base::win::SessionInfo session_info(session_id);
    if (!session_info.isValid())
    {
        LOG(LS_ERROR) << "Unable to get session info (sid=" << session_id << ")";
        return;
    }

    LOG(LS_INFO) << "# Session info (sid=" << session_id
                 << " username='" << session_info.userName() << "'"
                 << " connect_state=" << base::win::SessionInfo::connectStateToString(session_info.connectState())
                 << " win_station='" << session_info.winStationName() << "'"
                 << " domain='" << session_info.domain() << "'"
                 << " locked=" << session_info.isUserLocked() << ")";

    base::win::ScopedHandle user_token;
    if (!createLoggedOnUserToken(session_id, &user_token))
    {
        LOG(LS_ERROR) << "Failed to get user token (sid=" << session_id << ")";
        return;
    }

    if (!user_token.isValid())
    {
        LOG(LS_INFO) << "User is not logged in (sid=" << session_id << ")";

        // If there is no user logged in, but the session exists, then add the session without
        // connecting to UI (we cannot start UI if the user is not logged in).
        addUserSession(FROM_HERE, session_id, nullptr);
        return;
    }

    if (session_info.isUserLocked())
    {
        LOG(LS_INFO) << "Session has LOCKED user (sid=" << session_id << ")";

        // If the user session is locked, then we do not start the UI process. The process will be
        // started later when the user session is unlocked.
        addUserSession(FROM_HERE, session_id, nullptr);
        return;
    }
    else
    {
        LOG(LS_INFO) << "Session has UNLOCKED user (sid=" << session_id << ")";
    }

    std::filesystem::path file_path;
    if (!base::BasePaths::currentExecDir(&file_path))
    {
        LOG(LS_ERROR) << "Failed to get current exec directory (sid=" << session_id << ")";
        return;
    }

    file_path.append(kExecutableNameForUi);

    base::CommandLine command_line(file_path);
    command_line.appendSwitch(u"hidden");

    if (!createProcessWithToken(user_token, command_line))
    {
        LOG(LS_ERROR) << "Failed to start process with user token (sid=" << session_id
                        << " cmd=" << command_line.commandLineString() << ")";
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
                    LOG(LS_ERROR) << "Failed to get current exec directory (sid=" << session_id << ")";
                    return;
                }

                file_path.append(kExecutableNameForUi);

                std::string command_line =
                    fmt::format("sudo DISPLAY=':0' FONTCONFIG_PATH=/etc/fonts "
                                "DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/$(id -u {0})/bus "
                                "-u {0} {1} --hidden &",
                                user_name,
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
void UserSessionManager::addUserSession(const base::Location& location, base::SessionId session_id,
                                        base::IpcChannel* channel)
{
    LOG(LS_INFO) << "Add user session: " << session_id << " (from=" << location.toString() << ")";

    for (const auto& session : sessions_)
    {
        if (session->sessionId() == session_id)
        {
            LOG(LS_INFO) << "Restart user session: " << session_id;
            session->restart(channel);
            return;
        }
    }

    std::unique_ptr<UserSession> user_session = std::make_unique<UserSession>(
        task_runner_, session_id, channel, this);

    LOG(LS_INFO) << "Start user session: " << session_id;
    sessions_.emplace_back(std::move(user_session));
    sessions_.back()->start(router_state_);
}

} // namespace host
