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
            this, &UserSession::onUserSessionEvent);
}

//--------------------------------------------------------------------------------------------------
UserSession::~UserSession()
{
    LOG(INFO) << "Dtor";
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

    connect(ipc_server_, &base::IpcServer::sig_newConnection,
            this, &UserSession::onIpcNewConnection);

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
    base::SessionId session_id = 0;

#if defined(Q_OS_WINDOWS)
    session_id = base::activeConsoleSessionId();
    if (session_id == base::kInvalidSessionId)
    {
        LOG(ERROR) << "Unable to get active console session id";
        return false;
    }
#endif

    attach(FROM_HERE, session_id);
    return true;
}

//--------------------------------------------------------------------------------------------------
void UserSession::onRouterStateChanged(const proto::internal::RouterState& router_state)
{
    outgoing_message_.newMessage().mutable_router_state()->CopyFrom(router_state);
    sendSessionMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSession::onUpdateCredentials(base::HostId host_id, const QString& password)
{
    proto::internal::Credentials* credentials = outgoing_message_.newMessage().mutable_credentials();
    credentials->set_host_id(host_id);
    credentials->set_password(password.toStdString());
    sendSessionMessage();
}

//--------------------------------------------------------------------------------------------------
bool UserSession::isAttached() const
{
    return ipc_channel_ != nullptr;
}

//--------------------------------------------------------------------------------------------------
void UserSession::onSwitchSession(base::SessionId session_id)
{
    LOG(INFO) << "Switch session:" << session_id;

    is_console_ = session_id == base::activeConsoleSessionId();
    dettach(FROM_HERE);
    attach(FROM_HERE, session_id);
}

//--------------------------------------------------------------------------------------------------
void UserSession::onClientConfirmation(const proto::internal::ConfirmationRequest& request)
{
    if (request.session_type() == proto::peer::SESSION_TYPE_SYSTEM_INFO)
    {
        LOG(INFO) << "Accept: confirmation for system info session NOT required";
        emit sig_confirmationReply(request.id(), true);
        return;
    }

    SystemSettings settings;

    if (!isAttached())
    {
        LOG(INFO) << "No active GUI process";

        base::SessionInfo session_info(session_id_);
        if (!session_info.isValid())
        {
            LOG(ERROR) << "Reject: unable to get session info";
            emit sig_confirmationReply(request.id(), false);
            return;
        }

        if (session_info.connectState() != base::SessionInfo::ConnectState::ACTIVE)
        {
            if (settings.noUserAction() == SystemSettings::NoUserAction::ACCEPT)
            {
                LOG(INFO) << "Accept: no active user";
                emit sig_confirmationReply(request.id(), true);
            }
            else
            {
                LOG(INFO) << "Reject: no active user";
                emit sig_confirmationReply(request.id(), false);
            }
            return;
        }

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

    // If the GUI process is available, then we ask it if the connection is allowed.
    outgoing_message_.newMessage().mutable_confirmation_request()->CopyFrom(request);

    // Send confirmation request to GUI.
    sendSessionMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSession::onClientStarted(quint32 client_id, proto::peer::SessionType session_type,
    const QString& computer_name, const QString& display_name)
{
    proto::internal::ConnectEvent* event = outgoing_message_.newMessage().mutable_connect_event();
    event->set_client_id(client_id);
    event->set_session_type(session_type);
    event->set_computer_name(computer_name.toStdString());
    event->set_display_name(display_name.toStdString());
    sendSessionMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSession::onClientFinished(quint32 client_id)
{
    outgoing_message_.newMessage().mutable_disconnect_event()->set_client_id(client_id);
    sendSessionMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSession::onClientChat(quint32 client_id, const proto::chat::Chat& chat)
{
    outgoing_message_.newMessage().mutable_chat()->CopyFrom(chat);
    sendSessionMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSession::onClientRecording(const QString& computer, const QString& user, bool started)
{
    proto::internal::RecordingState* recording_state =
        outgoing_message_.newMessage().mutable_recording_state();
    recording_state->set_computer_name(computer.toStdString());
    recording_state->set_user_name(user.toStdString());
    recording_state->set_started(started);
    sendSessionMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSession::onUserSessionEvent(quint32 status, quint32 session_id)
{
#if defined(Q_OS_WINDOWS)
    LOG(INFO) << "State: attached=" << isAttached() << "session_id=" << session_id_
              << "is_console=" << is_console_;

    switch (status)
    {
        case WTS_CONSOLE_CONNECT:
        {
            if (!is_console_)
                return;

            attach(FROM_HERE, session_id);
        }
        break;

        case WTS_CONSOLE_DISCONNECT:
        {
            if (!is_console_)
                return;

            dettach(FROM_HERE);
        }
        break;

        case WTS_REMOTE_DISCONNECT:
        {
            if (is_console_)
                return;

            is_console_ = true;
            dettach(FROM_HERE);
            attach(FROM_HERE, base::activeConsoleSessionId());
        }
        break;

        case WTS_SESSION_LOGON:
        case WTS_SESSION_UNLOCK:
        {
            if (!isAttached() && session_id == session_id_)
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
void UserSession::onIpcNewConnection()
{
    LOG(INFO) << "New IPC connection";
    CHECK(ipc_server_);

    if (!ipc_server_->hasPendingConnections())
    {
        LOG(ERROR) << "No pending connections in IPC server";
        return;
    }

    ipc_channel_ = ipc_server_->nextPendingConnection();
    ipc_channel_->setParent(this);

    connect(ipc_channel_, &base::IpcChannel::sig_disconnected,
            this, &UserSession::onIpcDisconnected);
    connect(ipc_channel_, &base::IpcChannel::sig_messageReceived,
            this, &UserSession::onIpcMessageReceived);

    attach_timer_->stop();
    ipc_channel_->resume();

    LOG(INFO) << "Start user session";
    emit sig_attached();
}

//--------------------------------------------------------------------------------------------------
void UserSession::onIpcDisconnected()
{
    dettach(FROM_HERE);
}

//--------------------------------------------------------------------------------------------------
void UserSession::onIpcMessageReceived(quint32 channel_id, const QByteArray& buffer)
{
    if (channel_id != proto::internal::CHANNEL_ID_SESSION)
    {
        LOG(WARNING) << "Unhandled message from channel" << channel_id;
        return;
    }

    if (!incoming_message_.parse(buffer))
    {
        LOG(ERROR) << "Invalid message from UI";
        return;
    }

    if (incoming_message_->has_credentials_request())
    {
        const proto::internal::CredentialsRequest& request = incoming_message_->credentials_request();
        proto::internal::CredentialsRequest::Type type = request.type();

        if (type == proto::internal::CredentialsRequest::NEW_PASSWORD)
        {
            LOG(INFO) << "New credentials requested";
            emit sig_changeOneTimePassword();
        }
        else
        {
            DCHECK_EQ(type, proto::internal::CredentialsRequest::REFRESH);
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
        proto::internal::ConfirmationReply confirmation =
            incoming_message_->confirmation_reply();

        LOG(INFO) << "Connect confirmation (request_id:" << confirmation.id() << "accept:"
                  << confirmation.accept();

        emit sig_confirmationReply(confirmation.id(), confirmation.accept());
    }
    else if (incoming_message_->has_control())
    {
        const proto::internal::ServiceControl& control = incoming_message_->control();

        switch (control.code())
        {
            case proto::internal::ServiceControl::CODE_KILL:
            {
                CHECK(control.has_unsigned_integer());
                LOG(INFO) << "ServiceControl::CODE_KILL (client_id" << control.unsigned_integer() << ")";
                emit sig_stopClient(static_cast<quint32>(control.unsigned_integer()));
            }
            break;

            case proto::internal::ServiceControl::CODE_PAUSE:
            {
                CHECK(control.has_boolean());
                LOG(INFO) << "ServiceControl::CODE_PAUSE (paused" << control.boolean() << ")";
                emit sig_pauseChanged(control.boolean());
            }
            break;

            case proto::internal::ServiceControl::CODE_LOCK_MOUSE:
            {
                CHECK(control.has_boolean());
                LOG(INFO) << "ServiceControl::CODE_LOCK_MOUSE (lock_mouse" << control.boolean() << ")";
                emit sig_lockMouseChanged(control.boolean());
            }
            break;

            case proto::internal::ServiceControl::CODE_LOCK_KEYBOARD:
            {
                CHECK(control.has_boolean());
                LOG(INFO) << "ServiceControl::CODE_LOCK_KEYBOARD (lock_keyboard" << control.boolean() << ")";
                emit sig_lockKeyboardChanged(control.boolean());
            }
            break;

            default:
            {
                LOG(ERROR) << "Unhandled control code:" << control.code();
                return;
            }
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
void UserSession::attach(const base::Location& location, base::SessionId session_id)
{
    LOG(INFO) << "Attaching to UI process (sid" << session_id << "from" << location << ")";

    if (ipc_channel_)
    {
        LOG(ERROR) << "Already attached";
        return;
    }

    session_id_ = session_id;
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

    file_path.append('\\');
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
void UserSession::dettach(const base::Location& location)
{
    LOG(INFO) << "Dettached from" << location;

    if (ipc_channel_)
    {
        ipc_channel_->disconnect(this);
        ipc_channel_->deleteLater();
        ipc_channel_ = nullptr;
    }

    session_id_ = base::kInvalidSessionId;
    emit sig_dettached();
}

//--------------------------------------------------------------------------------------------------
void UserSession::sendSessionMessage()
{
    if (!ipc_channel_)
    {
        LOG(INFO) << "IPC channel is not connected";
        return;
    }

    ipc_channel_->send(proto::internal::CHANNEL_ID_SESSION, outgoing_message_.serialize());
}

} // namespace host
