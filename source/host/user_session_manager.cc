//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include <QTimer>

#include "base/application.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/ipc/ipc_channel.h"
#include "host/host_storage.h"
#include "host/system_settings.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/scoped_object.h"
#include "base/win/session_info.h"
#include "base/win/session_status.h"
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
    : QObject(parent),
      attach_timer_(new QTimer(this))
{
    LOG(INFO) << "Ctor";

    attach_timer_->setSingleShot(true);
    attach_timer_->setInterval(std::chrono::seconds(15));

    connect(attach_timer_, &QTimer::timeout, this, [this]()
    {
        dettach(FROM_HERE);
    });

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

    QString ipc_channel_name = base::IpcServer::createUniqueId();

    HostStorage ipc_storage;
    ipc_storage.setChannelIdForUI(ipc_channel_name);

    LOG(INFO) << "Start IPC server for UI (channel_id=" << ipc_channel_name << ")";

    // Start the server which will accept incoming connections from UI processes in user sessions.
    if (!ipc_server_->start(ipc_channel_name))
    {
        LOG(ERROR) << "Failed to start IPC server for UI (channel_id=" << ipc_channel_name << ")";
        return false;
    }

    LOG(INFO) << "IPC channel for UI started";

#if defined(Q_OS_WINDOWS)
    base::SessionId session_id = base::activeConsoleSessionId();
    if (session_id == base::kInvalidSessionId)
    {
        LOG(ERROR) << "Unable to get active console session id";
        return false;
    }

    LOG(INFO) << "Starting process for session id:" << session_id;

    // Start UI process in user session.
    attach(FROM_HERE, session_id);
#else
    attach(FROM_HERE, 0);
#endif

    return true;
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onRouterStateChanged(const proto::internal::RouterState& router_state)
{
    LOG(INFO) << "Router state changed:" << router_state << "sid" << session_id_ << ")";

    if (!isConnected())
    {
        LOG(INFO) << "Not connected to UI";
        return;
    }

    outgoing_message_.newMessage().mutable_router_state()->CopyFrom(router_state);
    sendSessionMessage();

    emit sig_credentialsRequested();
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onUpdateCredentials(base::HostId host_id, const QString& password)
{
    LOG(INFO) << "Set host ID for session:" << host_id;

    if (!isConnected())
    {
        LOG(INFO) << "Not connected to UI";
        return;
    }

    if (host_id == base::kInvalidHostId)
    {
        LOG(ERROR) << "Invalid host ID (sid" << session_id_ << ")";
        return;
    }

    proto::internal::Credentials* credentials = outgoing_message_.newMessage().mutable_credentials();
    credentials->set_host_id(host_id);
    credentials->set_password(password.toStdString());

    sendSessionMessage();
}

//--------------------------------------------------------------------------------------------------
bool UserSessionManager::isConnected() const
{
    return ipc_channel_ != nullptr;
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onAskForConfirmation(
    base::SessionId session_id, const proto::internal::ConnectConfirmationRequest& request)
{
    if (request.session_type() == proto::peer::SESSION_TYPE_SYSTEM_INFO)
    {
        LOG(INFO) << "Accept: confirmation for system info session NOT required";
        emit sig_askForConfirmation(request.id(), true);
        return;
    }

    SystemSettings settings;

    if (!isConnected())
    {
        LOG(INFO) << "No active GUI process";

        base::SessionInfo session_info(session_id);
        if (!session_info.isValid())
        {
            LOG(ERROR) << "Reject: unable to get session info";
            emit sig_askForConfirmation(request.id(), false);
            return;
        }

        if (session_info.connectState() != base::SessionInfo::ConnectState::ACTIVE)
        {
            if (settings.noUserAction() == SystemSettings::NoUserAction::ACCEPT)
            {
                LOG(INFO) << "Accept: no active user";
                emit sig_askForConfirmation(request.id(), true);
            }
            else
            {
                LOG(INFO) << "Reject: no active user";
                emit sig_askForConfirmation(request.id(), false);
            }
            return;
        }

        LOG(INFO) << "Reject: user is active, but there is no connection to the GUI";
        emit sig_askForConfirmation(request.id(), false);
        return;
    }

    if (!settings.connectConfirmation())
    {
        LOG(INFO) << "Accept: connect confirmation is disabled";
        emit sig_askForConfirmation(request.id(), true);
        return;
    }

    // If the GUI process is available, then we ask it if the connection is allowed.
    proto::internal::ConnectConfirmationRequest* ipc_request =
        outgoing_message_.newMessage().mutable_connect_confirmation_request();
    ipc_request->CopyFrom(request);

    // Send confirmation request to GUI.
    sendSessionMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onClientStarted(const proto::internal::ConnectEvent& event)
{
    outgoing_message_.newMessage().mutable_connect_event()->CopyFrom(event);
    sendSessionMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onClientFinished(const proto::internal::DisconnectEvent& event)
{
    outgoing_message_.newMessage().mutable_disconnect_event()->CopyFrom(event);
    sendSessionMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onClientSessionTextChat(
    quint32 client_id, const proto::text_chat::TextChat& text_chat)
{
    if (!isConnected())
    {
        LOG(INFO) << "Not connected to UI";
        return;
    }

    outgoing_message_.newMessage().mutable_text_chat()->CopyFrom(text_chat);
    sendSessionMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onClientSessionVideoRecording(
    const QString& computer_name, const QString& user_name, bool started)
{
    proto::internal::VideoRecordingState* video_recording_state =
        outgoing_message_.newMessage().mutable_video_recording_state();
    video_recording_state->set_computer_name(computer_name.toStdString());
    video_recording_state->set_user_name(user_name.toStdString());
    video_recording_state->set_started(started);

    sendSessionMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onUserSessionEvent(quint32 status, quint32 session_id)
{
#if defined(Q_OS_WINDOWS)
    LOG(INFO) << "User session event (status:" << base::sessionStatusToString(status)
              << "session_id:" << session_id << ")";

    switch (status)
    {
        case WTS_CONSOLE_CONNECT:
        case WTS_REMOTE_CONNECT:
        case WTS_SESSION_LOGON:
        {
            // Start UI process in user session.
            emit sig_credentialsRequested();
            attach(FROM_HERE, session_id);
        }
        break;

        case WTS_SESSION_UNLOCK:
        {
            if (!isConnected() && session_id == session_id_)
                attach(FROM_HERE, session_id);
        }
        break;

        default:
            // Ignore other events.
            break;
    }
#endif // defined(Q_OS_WINDOWS)
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

    ipc_channel_ = ipc_server_->nextPendingConnection();
    ipc_channel_->setParent(this);

    connect(ipc_channel_, &base::IpcChannel::sig_disconnected,
            this, &UserSessionManager::onIpcDisconnected);
    connect(ipc_channel_, &base::IpcChannel::sig_messageReceived,
            this, &UserSessionManager::onIpcMessageReceived);

#if defined(Q_OS_WINDOWS)
    session_id_ = ipc_channel_->peerSessionId();
    if (session_id_ == base::kInvalidSessionId)
    {
        LOG(ERROR) << "Invalid session id";
        return;
    }

    base::SessionId console_session_id = base::activeConsoleSessionId();
    if (console_session_id == base::kInvalidSessionId)
    {
        LOG(ERROR) << "Invalid console session ID (sid" << session_id_ << ")";
    }
    else
    {
        LOG(INFO) << "Console session ID:" << console_session_id;
    }

    is_console_ = session_id_ != console_session_id;
#endif

    attach_timer_->stop();
    ipc_channel_->resume();

    LOG(INFO) << "Start user session:" << session_id_;
    emit sig_attached();
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onIpcDisconnected()
{
    LOG(INFO) << "Ipc channel disconnected (sid" << session_id_ << ")";
    dettach(FROM_HERE);
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::onIpcMessageReceived(quint32 channel_id, const QByteArray& buffer)
{
    if (channel_id != proto::internal::CHANNEL_ID_SESSION)
    {
        LOG(WARNING) << "Unhandled message from channel" << channel_id;
        return;
    }

    if (!incoming_message_.parse(buffer))
    {
        LOG(ERROR) << "Invalid message from UI (sid" << session_id_ << ")";
        return;
    }

    if (incoming_message_->has_credentials_request())
    {
        const proto::internal::CredentialsRequest& request = incoming_message_->credentials_request();
        proto::internal::CredentialsRequest::Type type = request.type();

        if (type == proto::internal::CredentialsRequest::NEW_PASSWORD)
        {
            LOG(INFO) << "New credentials requested (sid" << session_id_ << ")";
            emit sig_changeOneTimePassword();
        }
        else
        {
            DCHECK_EQ(type, proto::internal::CredentialsRequest::REFRESH);
            LOG(INFO) << "Credentials update requested (sid" << session_id_ << ")";
        }
    }
    else if (incoming_message_->has_one_time_sessions())
    {
        quint32 sessions = incoming_message_->one_time_sessions().sessions();
        emit sig_changeOneTimeSessions(sessions);
    }
    else if (incoming_message_->has_connect_confirmation())
    {
        proto::internal::ConnectConfirmation connect_confirmation =
            incoming_message_->connect_confirmation();

        LOG(INFO) << "Connect confirmation (id=" << connect_confirmation.id() << "accept="
                  << connect_confirmation.accept_connection() << "sid=" << session_id_ << ")";

        emit sig_askForConfirmation(connect_confirmation.id(), connect_confirmation.accept_connection());
    }
    else if (incoming_message_->has_control())
    {
        const proto::internal::ServiceControl& control = incoming_message_->control();

        switch (control.code())
        {
            case proto::internal::ServiceControl::CODE_KILL:
            {
                if (!control.has_unsigned_integer())
                {
                    LOG(ERROR) << "Invalid parameter for CODE_KILL (sid" << session_id_ << ")";
                    return;
                }

                LOG(INFO) << "ServiceControl::CODE_KILL (sid" << session_id_ << "client_id"
                          << control.unsigned_integer() << ")";
                emit sig_stopClient(static_cast<quint32>(control.unsigned_integer()));
            }
            break;

            case proto::internal::ServiceControl::CODE_PAUSE:
            {
                if (!control.has_boolean())
                {
                    LOG(ERROR) << "Invalid parameter for CODE_PAUSE (sid" << session_id_ << ")";
                    return;
                }

                LOG(INFO) << "ServiceControl::CODE_PAUSE (sid" << session_id_ << "paused"
                          << control.boolean() << ")";
                emit sig_pauseChanged(control.boolean());
            }
            break;

            case proto::internal::ServiceControl::CODE_LOCK_MOUSE:
            {
                if (!control.has_boolean())
                {
                    LOG(ERROR) << "Invalid parameter for CODE_LOCK_MOUSE (sid" << session_id_ << ")";
                    return;
                }

                LOG(INFO) << "ServiceControl::CODE_LOCK_MOUSE (sid" << session_id_
                          << "lock_mouse" << control.boolean() << ")";
                emit sig_lockMouseChanged(control.boolean());
            }
            break;

            case proto::internal::ServiceControl::CODE_LOCK_KEYBOARD:
            {
                if (!control.has_boolean())
                {
                    LOG(ERROR) << "Invalid parameter for CODE_LOCK_KEYBOARD (sid" << session_id_ << ")";
                    return;
                }

                LOG(INFO) << "ServiceControl::CODE_LOCK_KEYBOARD (sid" << session_id_
                          << "lock_keyboard" << control.boolean() << ")";
                emit sig_lockKeyboardChanged(control.boolean());
            }
            break;

            default:
            {
                LOG(ERROR) << "Unhandled control code:" << control.code() << "(sid" << session_id_ << ")";
                return;
            }
        }
    }
    else if (incoming_message_->has_text_chat())
    {
        LOG(INFO) << "Text chat message (sid" << session_id_ << ")";
        emit sig_textChatMessage(incoming_message_->text_chat());
    }
    else
    {
        LOG(ERROR) << "Unhandled message from UI (sid" << session_id_ << ")";
    }
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::attach(const base::Location& location, base::SessionId session_id)
{
    LOG(INFO) << "Attaching to UI process (sid" << session_id << "from" << location << ")";

    attach_timer_->start();

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
              << "locked=" << session_info.isUserLocked()
              << ")";

    if (session_info.connectState() != base::SessionInfo::ConnectState::ACTIVE)
    {
        LOG(INFO) << "Console session is not active. Launching the GUI is not required";
        return;
    }

    if (session_info.isUserLocked())
    {
        LOG(INFO) << "Console session is locked. Launching the GUI is not required";
        return;
    }

    base::ScopedHandle user_token;
    if (!createLoggedOnUserToken(session_id, &user_token))
    {
        LOG(ERROR) << "Failed to get user token (sid=" << session_id << ")";
        return;
    }

    if (!user_token.isValid())
    {
        LOG(INFO) << "User is not logged in (sid=" << session_id << ")";
        return;
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
void UserSessionManager::dettach(const base::Location& location)
{
    LOG(INFO) << "Dettached (sid" << session_id_ << "from" << location << ")";

    if (ipc_channel_)
    {
        ipc_channel_->disconnect(this);
        ipc_channel_->deleteLater();
        ipc_channel_ = nullptr;
    }

    emit sig_dettached();
}

//--------------------------------------------------------------------------------------------------
void UserSessionManager::sendSessionMessage()
{
    if (!ipc_channel_)
    {
        LOG(INFO) << "IPC channel not exists (sid" << session_id_ << ")";
        return;
    }

    ipc_channel_->send(proto::internal::CHANNEL_ID_SESSION, outgoing_message_.serialize());
}

} // namespace host
