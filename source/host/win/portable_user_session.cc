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

#include "host/win/portable_user_session.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QTimer>

#include "base/logging.h"
#include "base/numeric_utils.h"
#include "base/process_util.h"
#include "base/crypto/secure_memory.h"
#include "base/crypto/secure_string.h"
#include "base/ipc/ipc_channel.h"
#include "base/ipc/ipc_server.h"
#include "host/client.h"
#include "host/host_constants.h"

namespace {

const Seconds kAttachTimeout{ 30 };

} // namespace

//--------------------------------------------------------------------------------------------------
PortableUserSession::PortableUserSession(
    const QString& ipc_channel_id, SessionId gui_session_id, QObject* parent)
    : QObject(parent),
      ipc_channel_id_(ipc_channel_id),
      gui_session_id_(gui_session_id)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
PortableUserSession::~PortableUserSession()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void PortableUserSession::sendConnectEvent(quint32 client_id, proto::peer::SessionType session_type,
    const QString& computer_name, const QString& display_name)
{
    proto::user::ConnectEvent* event =
        outgoing_message_.newMessage<proto::user::ServiceToUser>().mutable_connect_event();
    event->set_client_id(client_id);
    event->set_session_type(session_type);
    event->set_computer_name(computer_name.toStdString());
    event->set_display_name(display_name.toStdString());
    sendMessage();
}

//--------------------------------------------------------------------------------------------------
void PortableUserSession::sendDisconnectEvent(quint32 client_id)
{
    outgoing_message_.newMessage<proto::user::ServiceToUser>()
        .mutable_disconnect_event()->set_client_id(client_id);
    sendMessage();
}

//--------------------------------------------------------------------------------------------------
bool PortableUserSession::start()
{
    if (ipc_server_)
    {
        LOG(ERROR) << "IPC server already exists";
        return false;
    }

    ipc_server_ = new IpcServer(this);
    connect(ipc_server_, &IpcServer::sig_newConnection, this, &PortableUserSession::onIpcNewConnection);

    LOG(INFO) << "Start IPC server for portable GUI (channel:" << ipc_channel_id_ << ")";

    if (!ipc_server_->start(ipc_channel_id_, IpcServer::AccessMode::INTERACTIVE_USER))
    {
        LOG(ERROR) << "Failed to start IPC server for portable GUI";
        return false;
    }

    LOG(INFO) << "IPC server for portable GUI is started";

    // The portable host lives only together with its GUI, so a GUI that never connects (it failed to
    // start or crashed) must not leave the host running in the background.
    QTimer::singleShot(kAttachTimeout, this, [this]()
    {
        if (ipc_channel_)
            return;

        LOG(ERROR) << "Portable GUI is not connected in time";
        emit sig_guiTerminated();
    });

    return true;
}

//--------------------------------------------------------------------------------------------------
void PortableUserSession::onRouterStateChanged(const proto::user::RouterState& state)
{
    outgoing_message_.newMessage<proto::user::ServiceToUser>().mutable_router_state()->CopyFrom(state);
    sendMessage();
}

//--------------------------------------------------------------------------------------------------
void PortableUserSession::onUpdateCredentials(HostId host_id, const SecureString& password)
{
    proto::user::Credentials* credentials =
        outgoing_message_.newMessage<proto::user::ServiceToUser>().mutable_credentials();
    credentials->set_host_id(host_id);
    credentials->set_password(password.toString().toStdString());

    sendMessage();

    // Security cleanup.
    memZero(credentials->mutable_password());
}

//--------------------------------------------------------------------------------------------------
void PortableUserSession::onClientStarted()
{
    Client* client = dynamic_cast<Client*>(sender());
    CHECK(client);

    proto::peer::SessionType session_type = client->sessionType();
    QString computer_name = client->computerName();
    QString display_name = client->displayName();
    quint32 client_id = client->clientId();

    switch (session_type)
    {
        case proto::peer::SESSION_TYPE_DESKTOP:
        case proto::peer::SESSION_TYPE_FILE_TRANSFER:
            sendConnectEvent(client_id, session_type, computer_name, display_name);
            break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void PortableUserSession::onClientFinished()
{
    Client* client = dynamic_cast<Client*>(sender());
    CHECK(client);

    proto::peer::SessionType session_type = client->sessionType();
    switch (session_type)
    {
        case proto::peer::SESSION_TYPE_DESKTOP:
        case proto::peer::SESSION_TYPE_FILE_TRANSFER:
            sendDisconnectEvent(client->clientId());
            break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void PortableUserSession::onClientMessage(quint8 net_channel_id, const QByteArray& buffer)
{
    if (!ipc_channel_ || buffer.isEmpty())
        return;

    quint32 channel_id = makeUint32(proto::user::CHANNEL_ID_NETWORK, net_channel_id);
    ipc_channel_->send(channel_id, buffer);
}

//--------------------------------------------------------------------------------------------------
void PortableUserSession::onIpcNewConnection()
{
    LOG(INFO) << "New IPC connection";
    CHECK(ipc_server_);

    if (!ipc_server_->hasPendingConnections())
    {
        LOG(ERROR) << "No pending connections in IPC server";
        return;
    }

    ScopedQPointer<IpcChannel> ipc_channel(ipc_server_->nextPendingConnection());
    ipc_channel->setParent(this);

    if (ipc_channel_)
    {
        LOG(WARNING) << "IPC channel is already connected";
        return;
    }

    // Verify the connecting peer's executable is exactly our own binary: the GUI is launched from the
    // same executable as this process in both the elevated (run-dir copy) and user modes.
    const QString expected_path =
        QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath();
    const QString actual_path = QFileInfo(
        ProcessUtil::filePath(ipc_channel->processId())).canonicalFilePath();
    if (actual_path.isEmpty() || actual_path != expected_path)
    {
        LOG(ERROR) << "IPC client has unexpected executable (pid:" << ipc_channel->processId()
                   << "path:" << actual_path << "expected:" << expected_path << ")";
        return;
    }

    // The channel is open to every authenticated user, so the same executable started by another user
    // must not take over the host.
    if (gui_session_id_ != kInvalidSessionId && ipc_channel->sessionId() != gui_session_id_)
    {
        LOG(ERROR) << "IPC client is in another session (pid:" << ipc_channel->processId()
                   << "session:" << ipc_channel->sessionId() << "expected:" << gui_session_id_ << ")";
        return;
    }

    ipc_channel_ = ipc_channel.release();
    session_id_ = ipc_channel_->sessionId();

    connect(ipc_channel_, &IpcChannel::sig_disconnected,
            this, &PortableUserSession::onIpcDisconnected);
    connect(ipc_channel_, &IpcChannel::sig_messageReceived,
            this, &PortableUserSession::onIpcMessageReceived);

    ipc_channel_->setPaused(false);

    LOG(INFO) << "Portable GUI connected (IPC channel" << ipc_channel_->channelName()
              << "session:" << session_id_ << ")";
    emit sig_attached();
}

//--------------------------------------------------------------------------------------------------
void PortableUserSession::onIpcDisconnected()
{
    LOG(INFO) << "Portable GUI disconnected";
    ipc_channel_.reset();
    session_id_ = kInvalidSessionId;
    emit sig_guiTerminated();
}

//--------------------------------------------------------------------------------------------------
void PortableUserSession::onIpcMessageReceived(
    quint32 channel_id, const QByteArray& buffer, bool /* reliable */)
{
    quint16 net_channel_id = lowWord(channel_id);
    quint16 ipc_channel_id = highWord(channel_id);

    if (ipc_channel_id == proto::user::CHANNEL_ID_NETWORK)
    {
        emit sig_userMessage(net_channel_id, buffer);
        return;
    }

    proto::user::UserToService* message =
        incoming_message_.parse<proto::user::UserToService>(buffer);
    if (!message)
    {
        LOG(ERROR) << "Invalid message from UI";
        return;
    }

    if (message->has_credentials_request())
    {
        const proto::user::CredentialsRequest& request = message->credentials_request();
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
    else if (message->has_one_time_sessions())
    {
        quint32 sessions = message->one_time_sessions().sessions();
        emit sig_changeOneTimeSessions(sessions);
    }
    else if (message->has_control())
    {
        const proto::user::ServiceControl& control = message->control();
        const std::string command_name = control.command_name();

        if (command_name == "stop_client")
        {
            if (!control.has_unsigned_integer())
            {
                LOG(ERROR) << "Malformed stop_client command";
                return;
            }
            LOG(INFO) << "stop_client" << control.unsigned_integer();
            emit sig_stopClient(static_cast<quint32>(control.unsigned_integer()));
        }
        else if (command_name == "pause")
        {
            if (!control.has_boolean())
            {
                LOG(ERROR) << "Malformed pause command";
                return;
            }
            LOG(INFO) << "pause" << control.boolean();
            emit sig_pauseChanged(control.boolean());
        }
        else if (command_name == "lock_mouse")
        {
            if (!control.has_boolean())
            {
                LOG(ERROR) << "Malformed lock_mouse command";
                return;
            }
            LOG(INFO) << "lock_mouse" << control.boolean();
            emit sig_lockMouseChanged(control.boolean());
        }
        else if (command_name == "lock_keyboard")
        {
            if (!control.has_boolean())
            {
                LOG(ERROR) << "Malformed lock_keyboard command";
                return;
            }
            LOG(INFO) << "lock_keyboard" << control.boolean();
            emit sig_lockKeyboardChanged(control.boolean());
        }
        else
        {
            LOG(ERROR) << "Unhandled command:" << command_name;
        }
    }
    else
    {
        LOG(ERROR) << "Unhandled message from UI";
    }
}

//--------------------------------------------------------------------------------------------------
void PortableUserSession::sendMessage()
{
    if (!ipc_channel_)
    {
        LOG(INFO) << "IPC channel is not connected";
        return;
    }

    ipc_channel_->send(
        proto::user::CHANNEL_ID_SERVICE, outgoing_message_.serialize<proto::user::ServiceToUser>());
}
