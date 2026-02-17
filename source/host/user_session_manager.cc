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

#include <QCoreApplication>
#include <QDir>

#include "base/application.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/ipc/ipc_channel.h"
#include "host/client_session.h"
#include "host/host_storage.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/scoped_object.h"
#include "base/win/session_enumerator.h"
#include "base/win/session_info.h"
#include <UserEnv.h>
#endif // defined(Q_OS_WINDOWS)

namespace host {

namespace {

#if defined(Q_OS_UNIX)
const char kExecutableNameForUi[] = "aspia_host";
#endif // defined(Q_OS_UNIX)

#if defined(Q_OS_WINDOWS)
const char kExecutableNameForUi[] = "aspia_host.exe";

// Name of the default session desktop.
const wchar_t kDefaultDesktopName[] = L"winsta0\\default";

//--------------------------------------------------------------------------------------------------
bool createLoggedOnUserToken(DWORD session_id, base::ScopedHandle* token_out)
{
    if (!WTSQueryUserToken(session_id, token_out->recieve()))
    {
        DWORD error_code = GetLastError();
        if (error_code == ERROR_NO_TOKEN)
        {
            token_out->reset();
            return true;
        }

        PLOG(ERROR) << "WTSQueryUserToken failed";
        return false;
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
                              CREATE_UNICODE_ENVIRONMENT | HIGH_PRIORITY_CLASS,
                              environment,
                              nullptr,
                              &startup_info,
                              &process_info))
    {
        PLOG(ERROR) << "CreateProcessAsUserW failed";
        if (!DestroyEnvironmentBlock(environment))
        {
            PLOG(ERROR) << "DestroyEnvironmentBlock failed";
        }
        return false;
    }

    if (!SetPriorityClass(process_info.hProcess, NORMAL_PRIORITY_CLASS))
    {
        PLOG(ERROR) << "SetPriorityClass failed";
    }

    if (!CloseHandle(process_info.hThread))
    {
        PLOG(ERROR) << "CloseHandle failed";
    }

    if (!CloseHandle(process_info.hProcess))
    {
        PLOG(ERROR) << "CloseHandle failed";
    }

    if (!DestroyEnvironmentBlock(environment))
    {
        PLOG(ERROR) << "DestroyEnvironmentBlock failed";
    }

    return true;
}
#endif // defined(Q_OS_WINDOWS)

} // namespace

//--------------------------------------------------------------------------------------------------
UserSessionManager::UserSessionManager(QObject* parent)
    : QObject(parent)
{
    LOG(INFO) << "Ctor";

    connect(base::Application::instance(), &base::Application::sig_sessionEvent,
            this, &UserSessionManager::onUserSessionEvent);
}

//--------------------------------------------------------------------------------------------------
UserSessionManager::~UserSessionManager()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
bool UserSessionManager::start()
{
    LOG(INFO) << "Starting user session manager";

    if (ipc_server_)
    {
        LOG(ERROR) << "IPC server already exists";
        return false;
    }

    ipc_server_ = new base::IpcServer(this);

    connect(ipc_server_, &base::IpcServer::sig_newConnection,
            this, &UserSessionManager::onIpcNewConnection);

    QString ipc_channel_for_ui = base::IpcServer::createUniqueId();
    HostStorage ipc_storage;
    ipc_storage.setChannelIdForUI(ipc_channel_for_ui);

    LOG(INFO) << "Start IPC server for UI (channel_id=" << ipc_channel_for_ui << ")";

    // Start the server which will accept incoming connections from UI processes in user sessions.
    if (!ipc_server_->start(ipc_channel_for_ui))
    {
        LOG(ERROR) << "Failed to start IPC server for UI (channel_id=" << ipc_channel_for_ui << ")";
        return false;
    }
    else
    {
        LOG(INFO) << "IPC channel for UI started";
    }

#if defined(Q_OS_WINDOWS)
    for (base::SessionEnumerator session; !session.isAtEnd(); session.advance())
    {
        base::SessionId session_id = session.sessionId();

        // Skip invalid session ID.
        if (session_id == base::kInvalidSessionId)
            continue;

        // Never run processes in a service session.
        if (session_id == base::kServiceSessionId)
            continue;

        QString name = session.sessionName();
        if (name.compare("console", Qt::CaseInsensitive) != 0)
        {
            LOG(INFO) << "RDP session detected";

            if (session.state() != WTSActive)
            {
                LOG(INFO) << "RDP session with ID" << session_id << "not in active state. "
                          << "Session process will not be started (name=" << name << "state="
                          << base::SessionEnumerator::stateToString(session.state()) << ")";
                continue;
            }
            else
            {
                LOG(INFO) << "RDP session in active state";
            }
        }
        else
        {
            LOG(INFO) << "CONSOLE session detected";
        }

        LOG(INFO) << "Starting process for session id:" << session_id << "(name=" << name
                  << "state=" << base::SessionEnumerator::stateToString(session.state())
                  << ")";

        // Start UI process in user session.
        startSessionProcess(FROM_HERE, session.sessionId());
    }
#else
    startSessionProcess(FROM_HERE, 0);
#endif

    LOG(INFO) << "User session manager is started";
    return true;
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onRouterStateChanged(const proto::internal::RouterState& router_state)
{
    LOG(INFO) << router_state;

    // Send an event of each session.
    for (const auto& session : std::as_const(sessions_))
        session->onRouterStateChanged(router_state);
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onUpdateCredentials(base::HostId host_id, const QString& password)
{
    LOG(INFO) << "Set host ID for session:" << host_id;

    // Send an event of each session.
    for (const auto& session : std::as_const(sessions_))
        session->onUpdateCredentials(host_id, password);
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onSettingsChanged()
{
    LOG(INFO) << "Settings changed";

    // Send an event of each session.
    for (const auto& session : std::as_const(sessions_))
        session->onSettingsChanged();
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onClientSession(ClientSession* client_session)
{
    LOG(INFO) << "Adding a new client connection (user:" << client_session->userName()
              << "host_id:" << client_session->hostId() << ")";

    std::unique_ptr<ClientSession> client_session_deleter(client_session);

    base::SessionId session_id = 0;

#if defined(Q_OS_WINDOWS)
    session_id = base::activeConsoleSessionId();
    if (session_id == base::kInvalidSessionId)
    {
        LOG(ERROR) << "Failed to get session id";
        return;
    }
#endif

    bool user_session_found = false;

    for (const auto& session : std::as_const(sessions_))
    {
        if (session->sessionId() != session_id)
            continue;

        if (session->state() == UserSession::State::DETTACHED)
        {
#if defined(Q_OS_WINDOWS)
            base::SessionInfo session_info(session_id);
            if (!session_info.isValid())
            {
                LOG(ERROR) << "Unable to determine session state. Connection aborted";
                return;
            }

            base::SessionInfo::ConnectState connect_state = session_info.connectState();

            switch (connect_state)
            {
                case base::SessionInfo::ConnectState::CONNECTED:
                {
                    LOG(INFO) << "Session exists, but there are no logged in users";
                    session->restart(nullptr);
                }
                break;

                default:
                    LOG(INFO) << "No connected UI. Connection rejected (connect_state="
                              << connect_state << ")";
                    return;
            }
#else
            session->restart(nullptr);
#endif
        }

        session->onClientSession(client_session_deleter.release());
        user_session_found = true;
        break;
    }

    if (!user_session_found)
    {
        LOG(ERROR) << "User session with id" << session_id << "not found";
    }
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onUserSessionEvent(quint32 status, quint32 session_id)
{
#if defined(Q_OS_WINDOWS)
    QString status_str = base::sessionStatusToString(status);

    LOG(INFO) << "User session event (status=" << status_str << "session_id=" << session_id << ")";

    // Send an event of each session.
    for (const auto& session : std::as_const(sessions_))
        session->onUserSessionEvent(status, session_id);

    switch (status)
    {
        case WTS_CONSOLE_CONNECT:
        case WTS_REMOTE_CONNECT:
        case WTS_SESSION_LOGON:
        {
            // Start UI process in user session.
            startSessionProcess(FROM_HERE, session_id);
        }
        break;

        case WTS_SESSION_UNLOCK:
        {
            for (const auto& session : std::as_const(sessions_))
            {
                if (session->sessionId() != session_id)
                    continue;

                if (!session->isConnectedToUi())
                {
                    // Start UI process in user session.
                    LOG(INFO) << "Starting session process for session:" << session_id;
                    startSessionProcess(FROM_HERE, session_id);
                }
                else
                {
                    LOG(INFO) << "Session proccess already connected for session:" << session_id;
                }
                break;
            }
        }
        break;

        default:
        {
            // Ignore other events.
        }
        break;
    }
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onUserSessionFinished()
{
    LOG(INFO) << "User session finished";

    for (auto it = sessions_.begin(); it != sessions_.end();)
    {
        UserSession* session = *it;

        if (session->state() != UserSession::State::FINISHED)
        {
            ++it;
            continue;
        }

        LOG(INFO) << "Finished session:" << session->sessionId();

        session->deleteLater();
        it = sessions_.erase(it);
    }
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onIpcNewConnection()
{
    LOG(INFO) << "New IPC connection";

    if (!ipc_server_)
    {
        LOG(ERROR) << "No IPC server instance!";
        return;
    }

    if (!ipc_server_->hasPendingConnections())
    {
        LOG(ERROR) << "No pending connections in IPC server";
        return;
    }

    base::IpcChannel* channel = ipc_server_->nextPendingConnection();
    base::SessionId session_id = 0;

#if defined(Q_OS_WINDOWS)
    session_id = channel->peerSessionId();
    if (session_id == base::kInvalidSessionId)
    {
        LOG(ERROR) << "Invalid session id";
        return;
    }
#endif

    addUserSession(FROM_HERE, session_id, channel);
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::startSessionProcess(
    const base::Location& location, base::SessionId session_id)
{
    LOG(INFO) << "Starting UI process (sid" << session_id << "from" << location << ")";

#if defined(Q_OS_WINDOWS)
    if (session_id == base::kInvalidSessionId)
    {
        LOG(ERROR) << "An attempt was detected to start a process in a INVALID session";
        return;
    }

    if (session_id == base::kServiceSessionId)
    {
        LOG(ERROR) << "An attempt was detected to start a process in a SERVICES session";
        return;
    }

    LOG(INFO) << "Starting user session";

    base::SessionInfo session_info(session_id);
    if (!session_info.isValid())
    {
        LOG(ERROR) << "Unable to get session info (sid=" << session_id << ")";
        return;
    }

    LOG(INFO) << "# Session info (sid=" << session_id
              << "username=" << session_info.userName()
              << "connect_state=" << session_info.connectState()
              << "win_station=" << session_info.winStationName()
              << "domain=" << session_info.domain()
              << "locked=" << session_info.isUserLocked() << ")";

    base::ScopedHandle user_token;
    if (!createLoggedOnUserToken(session_id, &user_token))
    {
        LOG(ERROR) << "Failed to get user token (sid=" << session_id << ")";
        return;
    }

    if (!user_token.isValid())
    {
        LOG(INFO) << "User is not logged in (sid=" << session_id << ")";

        // If there is no user logged in, but the session exists, then add the session without
        // connecting to UI (we cannot start UI if the user is not logged in).
        addUserSession(FROM_HERE, session_id, nullptr);
        return;
    }

    if (session_info.isUserLocked())
    {
        LOG(INFO) << "Session has LOCKED user (sid=" << session_id << ")";

        // If the user session is locked, then we do not start the UI process. The process will be
        // started later when the user session is unlocked.
        addUserSession(FROM_HERE, session_id, nullptr);
        return;
    }
    else
    {
        LOG(INFO) << "Session has UNLOCKED user (sid=" << session_id << ")";
    }

    QString file_path = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());

    file_path.append(QLatin1Char('\\'));
    file_path.append(kExecutableNameForUi);

    QString command_line = file_path + " --hidden";

    if (!createProcessWithToken(user_token, command_line))
    {
        LOG(ERROR) << "Failed to start process with user token (sid=" << session_id
                   << "cmd=" << command_line << ")";
    }
#elif defined(Q_OS_LINUX)
    std::error_code ignored_error;
    std::filesystem::directory_iterator it("/usr/share/xsessions/", ignored_error);
    if (it == std::filesystem::end(it))
    {
        LOG(ERROR) << "No X11 sessions";
        return;
    }

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("who", "r"), pclose);
    if (!pipe)
    {
        LOG(ERROR) << "Unable to open pipe";
        return;
    }

    std::array<char, 512> buffer;
    while (fgets(buffer.data(), buffer.size(), pipe.get()))
    {
        QString line = QString::fromLocal8Bit(buffer.data()).toLower();

        if (line.contains(":0") || line.contains("tty2"))
        {
            QStringList splitted = line.split(' ', Qt::SkipEmptyParts);

            if (!splitted.isEmpty())
            {
                QString user_name = splitted.front();
                QString file_path = QCoreApplication::applicationDirPath();
                file_path.append(kExecutableNameForUi);

                QByteArray command_line =
                    QString("sudo DISPLAY=':0' FONTCONFIG_PATH=/etc/fonts "
                            "DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/$(id -u %1)/bus "
                            "-u %1 %2 --hidden &").arg(user_name, file_path).toLocal8Bit();

                int ret = system(command_line.data());
                LOG(INFO) << "system result:" << ret;
            }
        }
    }
#else
    NOTIMPLEMENTED();
#endif
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::addUserSession(
    const base::Location& location, base::SessionId session_id, base::IpcChannel* channel)
{
    LOG(INFO) << "Add user session:" << session_id << "(from" << location << ")";

    for (const auto& session : std::as_const(sessions_))
    {
        if (session->sessionId() == session_id)
        {
            LOG(INFO) << "Restart user session:" << session_id;
            session->restart(channel);
            return;
        }
    }

    UserSession* user_session = new UserSession(session_id, channel, this);

    connect(user_session, &UserSession::sig_routerStateRequested,
            this, &UserSessionManager::sig_routerStateRequested);
    connect(user_session, &UserSession::sig_credentialsRequested,
            this, &UserSessionManager::sig_credentialsRequested);
    connect(user_session, &UserSession::sig_changeOneTimePassword,
            this, &UserSessionManager::sig_changeOneTimePassword);
    connect(user_session, &UserSession::sig_changeOneTimeSessions,
            this, &UserSessionManager::sig_changeOneTimeSessions);
    connect(user_session, &UserSession::sig_finished,
            this, &UserSessionManager::onUserSessionFinished, Qt::QueuedConnection);

    sessions_.append(user_session);

    LOG(INFO) << "Start user session:" << session_id;

    user_session->start();
}

} // namespace host
