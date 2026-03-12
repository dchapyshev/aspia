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

#include "host/user_session.h"

#include <QCoreApplication>
#include <QDir>
#include <QTimer>
#include <QVariant>

#include "base/application.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/ipc/ipc_channel.h"
#include "base/ipc/ipc_server.h"
#include "host/host_storage.h"
#include "host/system_settings.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/scoped_object.h"
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

    if (!CreateProcessAsUserW(token, nullptr, const_cast<wchar_t*>(qUtf16Printable(command_line)),
        nullptr, nullptr, FALSE, CREATE_UNICODE_ENVIRONMENT | HIGH_PRIORITY_CLASS, environment,
        nullptr, &startup_info, &process_info))
    {
        PLOG(ERROR) << "CreateProcessAsUserW failed";
        DestroyEnvironmentBlock(environment);
        return false;
    }

    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
    DestroyEnvironmentBlock(environment);
    return true;
}
#endif // defined(Q_OS_WINDOWS)

} // namespace

//--------------------------------------------------------------------------------------------------
UserSession::UserSession(QObject* parent)
    : QObject(parent),
      attach_timer_(new QTimer(this)),
      dettach_timer_(new QTimer(this)),
      startup_timer_(new QTimer(this))
{
    LOG(INFO) << "Ctor";

    attach_timer_->setSingleShot(true);
    attach_timer_->setInterval(std::chrono::seconds(15));

    dettach_timer_->setSingleShot(true);
    dettach_timer_->setInterval(std::chrono::seconds(15));

    startup_timer_->setSingleShot(true);
    startup_timer_->setInterval(std::chrono::seconds(1));

    connect(attach_timer_, &QTimer::timeout, this, [this]() { dettach(FROM_HERE); });
    connect(dettach_timer_, &QTimer::timeout, this, &UserSession::onDettachTimeout);
    connect(startup_timer_, &QTimer::timeout, this, &UserSession::onStartupUserCheck);
    connect(base::Application::instance(), &base::Application::sig_sessionEvent,
            this, &UserSession::onUserSessionEvent);
}

//--------------------------------------------------------------------------------------------------
UserSession::~UserSession()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void UserSession::sendConnectEvent(quint32 client_id, proto::peer::SessionType session_type,
    const QString &computer_name, const QString &display_name)
{
    proto::user::ConnectEvent* event = outgoing_message_.newMessage().mutable_connect_event();
    event->set_client_id(client_id);
    event->set_session_type(session_type);
    event->set_computer_name(computer_name.toStdString());
    event->set_display_name(display_name.toStdString());
    sendMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSession::sendDisconnectEvent(quint32 client_id)
{
    outgoing_message_.newMessage().mutable_disconnect_event()->set_client_id(client_id);
    sendMessage();
}

//--------------------------------------------------------------------------------------------------
bool UserSession::start()
{
    LOG(INFO) << "Starting user session";

    if (ipc_server_)
    {
        LOG(ERROR) << "IPC server already exists";
        return false;
    }

    ipc_server_ = new base::IpcServer(this);
    connect(ipc_server_, &base::IpcServer::sig_newConnection, this, &UserSession::onIpcNewConnection);

    QString ipc_channel_name = base::IpcServer::createUniqueId();

    HostStorage ipc_storage;
    ipc_storage.setChannelIdForUI(ipc_channel_name);

    LOG(INFO) << "Start IPC server for UI (channel_name:" << ipc_channel_name << ")";

    // Start the server which will accept incoming connections from UI processes in user sessions.
    if (!ipc_server_->start(ipc_channel_name))
    {
        LOG(ERROR) << "Failed to start IPC server for UI (channel_name:" << ipc_channel_name << ")";
        return false;
    }

    LOG(INFO) << "IPC server for UI is started";
    base::SessionId session_id = base::activeConsoleSessionId();
    if (session_id == base::kInvalidSessionId)
    {
        LOG(ERROR) << "Unable to get active console session id";
        return false;
    }

    attach(FROM_HERE, AttachReason::STARTUP, session_id);
    return true;
}

//--------------------------------------------------------------------------------------------------
void UserSession::onRouterStateChanged(const proto::user::RouterState& state)
{
    outgoing_message_.newMessage().mutable_router_state()->CopyFrom(state);
    sendMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSession::onUpdateCredentials(base::HostId host_id, const QString& password)
{
    proto::user::Credentials* credentials = outgoing_message_.newMessage().mutable_credentials();
    credentials->set_host_id(host_id);
    credentials->set_password(password.toStdString());
    sendMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSession::onClientSwitchSession(base::SessionId session_id)
{
    if (session_id == session_id_)
        return;

    LOG(INFO) << "Switch session:" << session_id;
    dettach(FROM_HERE);
    attach(FROM_HERE, AttachReason::SWITCH_SESSION, session_id);
}

//--------------------------------------------------------------------------------------------------
void UserSession::onClientConfirmation(const proto::user::ConfirmationRequest& request)
{
    if (request.session_type() == proto::peer::SESSION_TYPE_SYSTEM_INFO)
    {
        LOG(INFO) << "Accept: confirmation for system info session NOT required";
        emit sig_confirmationReply(request.id(), true);
        return;
    }

    SystemSettings settings;

#if defined(Q_OS_WINDOWS)
    if (state_ == State::DETTACHED)
    {
        LOG(INFO) << "No active GUI process";

        base::SessionId session_id = base::activeConsoleSessionId();
        if (session_id == base::kInvalidSessionId)
        {
            LOG(INFO) << "Reject: invalid console session id";
            emit sig_confirmationReply(request.id(), false);
            return;
        }

        base::SessionInfo session_info(session_id);
        if (!session_info.isValid())
        {
            LOG(ERROR) << "Reject: unable to get session info";
            emit sig_confirmationReply(request.id(), false);
            return;
        }

        if (session_info.connectState() == base::SessionInfo::ConnectState::ACTIVE)
        {
            LOG(INFO) << "Reject: user is active, but there is no connection to the GUI";
            emit sig_confirmationReply(request.id(), false);
            return;
        }

        if (!settings.connectConfirmation())
        {
            LOG(INFO) << "Accept: connect confirmation is disabled";
            emit sig_confirmationReply(request.id(), true);
            return;
        }

        if (settings.noUserAction() == SystemSettings::NoUserAction::ACCEPT)
        {
            LOG(INFO) << "Accept: no active user";
            emit sig_confirmationReply(request.id(), true);
            return;
        }

        LOG(INFO) << "Reject: no active user";
        emit sig_confirmationReply(request.id(), false);
        return;
    }
#endif // defined(Q_OS_WINDOWS)

    if (!settings.connectConfirmation())
    {
        LOG(INFO) << "Accept: connect confirmation is disabled";
        emit sig_confirmationReply(request.id(), true);
        return;
    }

    // If the GUI process is available, then we ask it if the connection is allowed.
    outgoing_message_.newMessage().mutable_confirmation_request()->CopyFrom(request);

    // Send confirmation request to GUI.
    sendMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSession::onClientStarted()
{
    QObject* client = sender();
    CHECK(client);

    auto session_type = static_cast<proto::peer::SessionType>(client->property("session_type").toUInt());
    QString computer_name = client->property("computer_name").toString();
    QString display_name = client->property("display_name").toString();
    quint32 client_id = client->property("client_id").toUInt();

    switch (session_type)
    {
        case proto::peer::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::peer::SESSION_TYPE_DESKTOP_VIEW:
        {
            ++desktop_client_count_;
            sendConnectEvent(client_id, session_type, computer_name, display_name);
        }
        break;

        case proto::peer::SESSION_TYPE_FILE_TRANSFER:
            sendConnectEvent(client_id, session_type, computer_name, display_name);
            break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void UserSession::onClientFinished()
{
    QObject* client = sender();
    CHECK(client);

    auto session_type = static_cast<proto::peer::SessionType>(client->property("session_type").toUInt());
    switch (session_type)
    {
        case proto::peer::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::peer::SESSION_TYPE_DESKTOP_VIEW:
        {
            sendDisconnectEvent(client->property("client_id").toUInt());

            desktop_client_count_ = std::max(desktop_client_count_ - 1, 0);
            if (desktop_client_count_)
                return;

            LOG(INFO) << "Last desktop client is disconnected";

            base::SessionId session_id = base::activeConsoleSessionId();
            if (session_id_ != session_id && session_id_ != base::kInvalidSessionId)
            {
                dettach(FROM_HERE);
                attach(FROM_HERE, AttachReason::OTHER, session_id);
            }
        }
        break;

        case proto::peer::SESSION_TYPE_FILE_TRANSFER:
            sendDisconnectEvent(client->property("client_id").toUInt());
            break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void UserSession::onClientChat(quint32 client_id, const proto::chat::Chat& chat)
{
    outgoing_message_.newMessage().mutable_chat()->CopyFrom(chat);
    sendMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSession::onClientRecording(bool started)
{
    QObject* client = sender();
    CHECK(client);

    QString computer_name = client->property("computer_name").toString();
    QString user_name = client->property("user_name").toString();

    proto::user::RecordingState* state = outgoing_message_.newMessage().mutable_recording_state();
    state->set_computer_name(computer_name.toStdString());
    state->set_user_name(user_name.toStdString());
    state->set_started(started);
    sendMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSession::onUserSessionEvent(quint32 status, quint32 session_id)
{
#if defined(Q_OS_WINDOWS)
    LOG(INFO) << "State (state:" << state_ << "session_id:" << session_id_ << "console:" << is_console_ << ")";

    switch (status)
    {
        case WTS_CONSOLE_CONNECT:
        {
            if (is_console_)
                attach(FROM_HERE, AttachReason::OTHER, session_id);
        }
        break;

        case WTS_CONSOLE_DISCONNECT:
        {
            if (is_console_)
                dettach(FROM_HERE);
        }
        break;

        case WTS_REMOTE_DISCONNECT:
        {
            if (is_console_ || session_id_ != session_id)
                return;

            dettach(FROM_HERE);
            attach(FROM_HERE, AttachReason::OTHER, base::activeConsoleSessionId());
        }
        break;

        case WTS_SESSION_LOGON:
        case WTS_SESSION_UNLOCK:
        {
            if (state_ == State::DETTACHED && is_console_ && session_id == base::activeConsoleSessionId())
                attach(FROM_HERE, AttachReason::OTHER, session_id);
        }
        break;

        default:
            // Ignore other events.
            break;
    }
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void UserSession::onIpcNewConnection()
{
    LOG(INFO) << "New IPC connection";
    CHECK(ipc_server_);

    if (!ipc_server_->hasPendingConnections())
    {
        LOG(ERROR) << "No pending connections in IPC server";
        return;
    }

    base::IpcChannel* ipc_channel = ipc_server_->nextPendingConnection();
    ipc_channel->setParent(this);

    if (ipc_channel_)
    {
        LOG(WARNING) << "IPC channel is already connected";
        ipc_channel->deleteLater();
        return;
    }

    if (session_id_ == base::kInvalidSessionId)
    {
        base::SessionId ipc_session_id = ipc_channel->sessionId();

        LOG(INFO) << "User GUI started by user with session id" << ipc_session_id;

        if (ipc_session_id != base::activeConsoleSessionId())
        {
            LOG(WARNING) << "Launching GUI is only possible in console session";
            ipc_channel->deleteLater();
            return;
        }

        session_id_ = ipc_session_id;
    }
    else
    {
        LOG(INFO) << "User GUI started by service";
    }

    ipc_channel_ = ipc_channel;

    connect(ipc_channel_, &base::IpcChannel::sig_disconnected, this, &UserSession::onIpcDisconnected);
    connect(ipc_channel_, &base::IpcChannel::sig_messageReceived, this, &UserSession::onIpcMessageReceived);

    dettach_timer_->stop();
    attach_timer_->stop();
    ipc_channel_->setPaused(false);

    LOG(INFO) << "Start user session (IPC channel" << ipc_channel_->channelName() << ")";

    state_ = State::ATTACHED;
    emit sig_attached();
}

//--------------------------------------------------------------------------------------------------
void UserSession::onIpcDisconnected()
{
#if defined(Q_OS_WINDOWS)
    base::SessionInfo session_info(session_id_);
    if (session_info.isValid())
    {
        if (session_info.connectState() == base::SessionInfo::ConnectState::ACTIVE &&
            !session_info.isUserLocked())
        {
            // The GUI process terminated while the user was in an active session.
            // There may be the following reasons:
            // 1. The user closed the application using the GUI.
            // 2. The user killed the process.
            // 3. The process has crashed.
            // Start a timer, after which all connected clients will be disconnected.

            LOG(WARNING) << "GUI process terminated during an active user session";
            dettach_timer_->start();
        }
    }
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void UserSession::onIpcMessageReceived(quint32 /* ipc_channel_id */, const QByteArray& buffer)
{
    if (!incoming_message_.parse(buffer))
    {
        LOG(ERROR) << "Invalid message from UI";
        return;
    }

    if (incoming_message_->has_credentials_request())
    {
        const proto::user::CredentialsRequest& request = incoming_message_->credentials_request();
        proto::user::CredentialsRequest::Type type = request.type();

        if (type == proto::user::CredentialsRequest::NEW_PASSWORD)
        {
            LOG(INFO) << "New credentials requested";
            emit sig_changeOneTimePassword();
        }
        else
        {
            DCHECK_EQ(type, proto::user::CredentialsRequest::REFRESH);
            LOG(INFO) << "Credentials update requested";
        }
    }
    else if (incoming_message_->has_one_time_sessions())
    {
        quint32 sessions = incoming_message_->one_time_sessions().sessions();
        emit sig_changeOneTimeSessions(sessions);
    }
    else if (incoming_message_->has_confirmation_reply())
    {
        proto::user::ConfirmationReply confirmation = incoming_message_->confirmation_reply();
        LOG(INFO) << "Connect confirmation (request_id:" << confirmation.id() << "accept:"
                  << confirmation.accept();
        emit sig_confirmationReply(confirmation.id(), confirmation.accept());
    }
    else if (incoming_message_->has_control())
    {
        const proto::user::ServiceControl& control = incoming_message_->control();
        const std::string command_name = control.command_name();

        if (command_name == "stop_client")
        {
            CHECK(control.has_unsigned_integer());
            LOG(INFO) << "stop_client" << control.unsigned_integer();
            emit sig_stopClient(static_cast<quint32>(control.unsigned_integer()));
        }
        else if (command_name == "pause")
        {
            CHECK(control.has_boolean());
            LOG(INFO) << "pause" << control.boolean();
            emit sig_pauseChanged(control.boolean());
        }
        else if (command_name == "lock_mouse")
        {
            CHECK(control.has_boolean());
            LOG(INFO) << "lock_mouse" << control.boolean();
            emit sig_lockMouseChanged(control.boolean());
        }
        else if (command_name == "lock_keyboard")
        {
            CHECK(control.has_boolean());
            LOG(INFO) << "lock_keyboard" << control.boolean();
            emit sig_lockKeyboardChanged(control.boolean());
        }
        else
        {
            LOG(ERROR) << "Unhandled command:" << command_name;
        }
    }
    else if (incoming_message_->has_chat())
    {
        LOG(INFO) << "Text chat message";
        emit sig_chatMessage(incoming_message_->chat());
    }
    else
    {
        LOG(ERROR) << "Unhandled message from UI";
    }
}

//--------------------------------------------------------------------------------------------------
void UserSession::onDettachTimeout()
{
    LOG(INFO) << "Stop all clients";
    dettach(FROM_HERE);
    emit sig_stopClient(0);
}

//--------------------------------------------------------------------------------------------------
void UserSession::onStartupUserCheck()
{
#if defined(Q_OS_WINDOWS)
    // Windows can automatically restore login (ARSO, Automatic Restart Sign On) when rebooting
    // (login as the user who was logged in before the reboot). When the service starts, we don't
    // see the logged in user, but after a while they appear (in a locked state) without any events.
    // Therefore, we check for a user logged in within the first 60 seconds (check is done 60 times
    // with an interval of 1 second) after the service starts. If we don't do this, the GUI will be
    // detected missing when a client connects, even though the user is still logged in, and the
    // client will be rejected.
    // Settings > Accounts > Sign-in options > Use my sign-in info to automatically finish...
    base::SessionInfo session_info(base::activeConsoleSessionId());
    if (session_info.isValid() && session_info.connectState() == base::SessionInfo::ConnectState::ACTIVE)
    {
        attach(FROM_HERE, AttachReason::OTHER, session_info.sessionId());
        return;
    }

    static int attempt_count = 0;

    if (attempt_count++ <= 60)
        startup_timer_->start();
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void UserSession::attach(const base::Location& location, AttachReason reason, base::SessionId session_id)
{
    LOG(INFO) << "Attaching to UI process (sid" << session_id << "from" << location << ")";

    if (state_ != State::DETTACHED)
    {
        LOG(ERROR) << "Already attached";
        return;
    }

    state_ = State::ATTACHING;
    is_console_ = session_id == base::activeConsoleSessionId();
    session_id_ = session_id;

    startup_timer_->stop();
    dettach_timer_->stop();
    attach_timer_->start();

#if defined(Q_OS_WINDOWS)
    if (session_id == base::kInvalidSessionId)
    {
        LOG(ERROR) << "An attempt was detected to start a process in a INVALID session";
        dettach(FROM_HERE);
        return;
    }

    if (session_id == base::kServiceSessionId)
    {
        LOG(ERROR) << "An attempt was detected to start a process in a SERVICES session";
        dettach(FROM_HERE);
        return;
    }

    base::SessionInfo session_info(session_id);
    if (!session_info.isValid())
    {
        LOG(ERROR) << "Unable to get session info (sid" << session_id << ")";
        dettach(FROM_HERE);
        return;
    }

    LOG(INFO) << "Session info (sid:" << session_id
              << "username:" << session_info.userName()
              << "connect_state:" << session_info.connectState()
              << "win_station:" << session_info.winStationName()
              << "domain:" << session_info.domain()
              << "locked:" << session_info.isUserLocked() << ")";

    base::ScopedHandle user_token;
    if (!createLoggedOnUserToken(session_id, &user_token))
    {
        LOG(ERROR) << "Failed to get user token (sid" << session_id << ")";
        dettach(FROM_HERE);
        return;
    }

    if (!user_token.isValid())
    {
        LOG(INFO) << "Console session is not active. Launching the GUI is not required";
        dettach(FROM_HERE);

        if (reason == AttachReason::STARTUP)
            startup_timer_->start();
        return;
    }

    QString file_path = QDir::toNativeSeparators(
        QCoreApplication::applicationDirPath() + '\\' + kExecutableNameForUi);

    QString command_line = file_path + " --hidden";

    if (!createProcessWithToken(user_token, command_line))
    {
        LOG(ERROR) << "Unable to start process with user token (sid" << session_id
                   << "cmd" << command_line << ")";
        dettach(FROM_HERE);
        return;
    }
#elif defined(Q_OS_LINUX)
    std::error_code ignored_error;
    std::filesystem::directory_iterator it("/usr/share/xsessions/", ignored_error);
    if (it == std::filesystem::end(it))
    {
        LOG(ERROR) << "No X11 sessions";
        dettach(FROM_HERE);
        return;
    }

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("who", "r"), pclose);
    if (!pipe)
    {
        LOG(ERROR) << "Unable to open pipe";
        dettach(FROM_HERE);
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
void UserSession::dettach(const base::Location& location)
{
    LOG(INFO) << "Dettached from" << location;

    if (ipc_channel_)
    {
        ipc_channel_->disconnect(this);
        ipc_channel_->deleteLater();
        ipc_channel_ = nullptr;
    }

    startup_timer_->stop();
    session_id_ = base::kInvalidSessionId;
    is_console_ = true;
    state_ = State::DETTACHED;
    emit sig_dettached();
}

//--------------------------------------------------------------------------------------------------
void UserSession::sendMessage()
{
    if (!ipc_channel_)
    {
        LOG(INFO) << "IPC channel is not connected";
        return;
    }

    ipc_channel_->send(0, outgoing_message_.serialize());
}

} // namespace host
