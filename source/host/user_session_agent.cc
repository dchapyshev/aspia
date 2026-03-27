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

#include "host/user_session_agent.h"

#include "base/logging.h"
#include "base/ipc/ipc_channel.h"
#include "host/host_storage.h"
#include "common/clipboard_monitor.h"

namespace host {

namespace {

auto g_statusType = qRegisterMetaType<host::UserSessionAgent::Status>();
auto g_clientListType = qRegisterMetaType<host::UserSessionAgent::ClientList>();

} // namespace

//--------------------------------------------------------------------------------------------------
UserSessionAgent::UserSessionAgent(QObject* parent)
    : QObject(parent)
{
    LOG(INFO) << "Ctor";
#if defined(Q_OS_WINDOWS)
    // 0x100-0x1FF Application reserved last shutdown range.
    if (!SetProcessShutdownParameters(0x100, SHUTDOWN_NORETRY))
        PLOG(ERROR) << "SetProcessShutdownParameters failed";
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
UserSessionAgent::~UserSessionAgent()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::onConnectToService()
{
    QString channel_name = HostStorage().channelIdForUI();
    LOG(INFO) << "Starting user session agent (channel_name:" << channel_name << ")";

    ipc_channel_ = new base::IpcChannel(this);

    connect(ipc_channel_, &base::IpcChannel::sig_connected, this, &UserSessionAgent::onIpcConnected);
    connect(ipc_channel_, &base::IpcChannel::sig_disconnected, this, &UserSessionAgent::onIpcDisconnected);
    connect(ipc_channel_, &base::IpcChannel::sig_errorOccurred, this, &UserSessionAgent::onIpcErrorOccurred);
    connect(ipc_channel_, &base::IpcChannel::sig_messageReceived, this, &UserSessionAgent::onIpcMessageReceived);

    ipc_channel_->connectTo(channel_name);
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::onUpdateCredentials(proto::user::CredentialsRequest_Type type)
{
    LOG(INFO) << "Update credentials request:" << type;
    proto::user::CredentialsRequest* request =
        outgoing_message_.newMessage().mutable_credentials_request();
    request->set_type(type);
    sendMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::onOneTimeSessions(quint32 sessions)
{
    LOG(INFO) << "One-time sessions changed:" << sessions;
    proto::user::OneTimeSessions* one_time_sessions =
        outgoing_message_.newMessage().mutable_one_time_sessions();
    one_time_sessions->set_sessions(sessions);
    sendMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::onStopClient(quint32 client_id)
{
    LOG(INFO) << "Stop client request:" << client_id;
    proto::user::ServiceControl* control = outgoing_message_.newMessage().mutable_control();
    control->set_command_name("stop_client");
    control->set_unsigned_integer(client_id);
    sendMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::onConnectConfirmation(quint32 id, bool accept)
{
    LOG(INFO) << "Connect confirmation (id:" << id << "accept:" << accept << ")";
    proto::user::ConfirmationReply* confirmation =
        outgoing_message_.newMessage().mutable_confirmation_reply();
    confirmation->set_id(id);
    confirmation->set_accept(accept);
    sendMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::onMouseLock(bool enable)
{
    LOG(INFO) << "Mouse lock:" << enable;
    proto::user::ServiceControl* control = outgoing_message_.newMessage().mutable_control();
    control->set_command_name("lock_mouse");
    control->set_boolean(enable);
    sendMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::onKeyboardLock(bool enable)
{
    LOG(INFO) << "Keyboard lock:" << enable;
    proto::user::ServiceControl* control = outgoing_message_.newMessage().mutable_control();
    control->set_command_name("lock_keyboard");
    control->set_boolean(enable);
    sendMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::onPause(bool enable)
{
    LOG(INFO) << "Pause:" << enable;
    proto::user::ServiceControl* control = outgoing_message_.newMessage().mutable_control();
    control->set_command_name("pause");
    control->set_boolean(enable);
    sendMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::onChat(const proto::chat::Chat& chat)
{
    LOG(INFO) << "Text chat message";
    outgoing_message_.newMessage().mutable_chat()->CopyFrom(chat);
    sendMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::onIpcConnected()
{
    LOG(INFO) << "IPC channel connected";
    emit sig_statusChanged(Status::CONNECTED_TO_SERVICE);
    ipc_channel_->setPaused(false);
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::onIpcDisconnected()
{
    LOG(INFO) << "IPC channel disconncted";

    if (ipc_channel_)
    {
        ipc_channel_->disconnect(this);
        ipc_channel_->deleteLater();
        ipc_channel_ = nullptr;
    }

    emit sig_statusChanged(Status::DISCONNECTED_FROM_SERVICE);
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::onIpcErrorOccurred()
{
    LOG(INFO) << "Unable to connect to IPC server";
    emit sig_statusChanged(Status::SERVICE_NOT_AVAILABLE);
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::onIpcMessageReceived(
    quint32 ipc_channel_id, const QByteArray& buffer, bool /* reliable */)
{
    if (ipc_channel_id == proto::user::CHANNEL_ID_CLIPBOARD)
    {
        proto::desktop::ClipboardMessage message;
        if (!base::parse(buffer, &message))
        {
            LOG(ERROR) << "Unable to parse clipboard message";
            return;
        }

        if (message.has_event())
        {
            if (clipboard_)
                clipboard_->injectClipboardEvent(message.event());
        }
        else
        {
            LOG(ERROR) << "Unhandled clipboard message";
        }
        return;
    }

    if (!incoming_message_.parse(buffer))
    {
        LOG(ERROR) << "Invalid message from service";
        return;
    }

    if (incoming_message_->has_confirmation_request())
    {
        const proto::user::ConfirmationRequest& request = incoming_message_->confirmation_request();

        LOG(INFO) << "Connect confirmation request received (id:" << request.id()
                  << "computer:" << request.computer_name() << "user:" << request.user_name() << ")";

        emit sig_confirmationRequest(request);
    }
    else if (incoming_message_->has_connect_event())
    {
        onConnectEvent(incoming_message_->connect_event());
    }
    else if (incoming_message_->has_disconnect_event())
    {
        onDisconnectEvent(incoming_message_->disconnect_event());
    }
    else if (incoming_message_->has_credentials())
    {
        const proto::user::Credentials& credentials = incoming_message_->credentials();
        LOG(INFO) << "Credentials received (host_id" << credentials.host_id() << ")";
        emit sig_credentialsChanged(credentials);
    }
    else if (incoming_message_->has_router_state())
    {
        const proto::user::RouterState router_state = incoming_message_->router_state();
        LOG(INFO) << "Router state received (" << router_state << ")";
        emit sig_routerStateChanged(router_state);
    }
    else if (incoming_message_->has_chat())
    {
        emit sig_chat(incoming_message_->chat());
    }
    else if (incoming_message_->has_recording_state())
    {
        const proto::user::RecordingState& recording_state =
            incoming_message_->recording_state();

        LOG(INFO) << "Video recording state changed (" << recording_state  << ")";

        emit sig_recordingStateChanged(
            QString::fromStdString(recording_state.computer_name()),
            QString::fromStdString(recording_state.user_name()),
            recording_state.started());
    }
    else
    {
        LOG(ERROR) << "Unhandled message from service";
    }
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::onConnectEvent(const proto::user::ConnectEvent& event)
{
    LOG(INFO) << "Connect event received";
    clients_.emplace_back(incoming_message_->connect_event());

    if (event.session_type() == proto::peer::SESSION_TYPE_DESKTOP_MANAGE && !clipboard_)
    {
        clipboard_ = new common::ClipboardMonitor(this);

        connect(clipboard_, &common::ClipboardMonitor::sig_clipboardEvent,
                this, &UserSessionAgent::onClipboardEvent);

        clipboard_->start();
    }

    emit sig_clientListChanged(clients_);
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::onDisconnectEvent(const proto::user::DisconnectEvent& event)
{
    LOG(INFO) << "Disconnect event received";

    for (auto it = clients_.begin(), it_end = clients_.end(); it != it_end; ++it)
    {
        if (it->client_id == event.client_id())
        {
            clients_.erase(it);
            break;
        }
    }

    bool has_desktop_manage = false;

    for (const auto& client : std::as_const(clients_))
    {
        if (client.session_type == proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
            has_desktop_manage = true;
    }

    if (!has_desktop_manage && clipboard_)
    {
        clipboard_->clearClipboard();
        clipboard_->disconnect(this);
        clipboard_->deleteLater();
        clipboard_ = nullptr;
    }

    emit sig_clientListChanged(clients_);
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::onClipboardEvent(const proto::desktop::ClipboardEvent& event)
{
    if (!ipc_channel_)
        return;

    proto::desktop::ClipboardMessage message;
    message.mutable_event()->CopyFrom(event);

    ipc_channel_->send(proto::user::CHANNEL_ID_CLIPBOARD, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::sendMessage()
{
    if (ipc_channel_)
        ipc_channel_->send(proto::user::CHANNEL_ID_USER, outgoing_message_.serialize());
}

} // namespace host
