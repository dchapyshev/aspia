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

#include "host/user_session_agent.h"

#include "base/logging.h"
#include "host/host_storage.h"

namespace host {

namespace {

auto g_statusType = qRegisterMetaType<host::UserSessionAgent::Status>();
auto g_clientListType = qRegisterMetaType<host::UserSessionAgent::ClientList>();

} // namespace

//--------------------------------------------------------------------------------------------------
UserSessionAgent::UserSessionAgent(QObject* parent)
    : QObject(parent)
{
    LOG(LS_INFO) << "Ctor";

#if defined(Q_OS_WINDOWS)
    // 0x100-0x1FF Application reserved last shutdown range.
    if (!SetProcessShutdownParameters(0x100, SHUTDOWN_NORETRY))
    {
        PLOG(LS_ERROR) << "SetProcessShutdownParameters failed";
    }
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
UserSessionAgent::~UserSessionAgent()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::onConnectToService()
{
    QString channel_id = HostStorage().channelIdForUI();
    LOG(LS_INFO) << "Starting user session agent (channel_id=" << channel_id << ")";

    ipc_channel_ = new base::IpcChannel(this);

    connect(ipc_channel_, &base::IpcChannel::sig_disconnected,
            this, &UserSessionAgent::onIpcDisconnected);
    connect(ipc_channel_, &base::IpcChannel::sig_messageReceived,
            this, &UserSessionAgent::onIpcMessageReceived);

    if (ipc_channel_->connectTo(channel_id))
    {
        LOG(LS_INFO) << "IPC channel connected (channel_id=" << channel_id << ")";
        emit sig_statusChanged(Status::CONNECTED_TO_SERVICE);
        ipc_channel_->resume();
    }
    else
    {
        LOG(LS_INFO) << "IPC channel not connected (channel_id=" << channel_id << ")";
        emit sig_statusChanged(Status::SERVICE_NOT_AVAILABLE);
    }
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::onUpdateCredentials(proto::internal::CredentialsRequest::Type request_type)
{
    LOG(LS_INFO) << "Update credentials request: " << request_type;

    if (!ipc_channel_)
    {
        LOG(LS_WARNING) << "No active IPC channel";
        return;
    }

    proto::internal::CredentialsRequest* request =
        outgoing_message_.newMessage().mutable_credentials_request();
    request->set_type(request_type);

    ipc_channel_->send(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::onOneTimeSessions(quint32 sessions)
{
    LOG(LS_INFO) << "One-time sessions changed: " << sessions;

    if (!ipc_channel_)
    {
        LOG(LS_WARNING) << "No active IPC channel";
        return;
    }

    proto::internal::OneTimeSessions* one_time_sessions =
        outgoing_message_.newMessage().mutable_one_time_sessions();
    one_time_sessions->set_sessions(sessions);

    ipc_channel_->send(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::onKillClient(quint32 id)
{
    LOG(LS_INFO) << "Kill client request: " << id;

    if (!ipc_channel_)
    {
        LOG(LS_WARNING) << "No active IPC channel";
        return;
    }

    proto::internal::ServiceControl* control = outgoing_message_.newMessage().mutable_control();

    control->set_code(proto::internal::ServiceControl::CODE_KILL);
    control->set_unsigned_integer(id);

    ipc_channel_->send(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::onConnectConfirmation(quint32 id, bool accept)
{
    LOG(LS_INFO) << "Connect confirmation (id=" << id << " accept=" << accept << ")";

    if (!ipc_channel_)
    {
        LOG(LS_WARNING) << "No active IPC channel";
        return;
    }

    proto::internal::ConnectConfirmation* confirmation =
        outgoing_message_.newMessage().mutable_connect_confirmation();
    confirmation->set_id(id);
    confirmation->set_accept_connection(accept);

    ipc_channel_->send(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::onMouseLock(bool enable)
{
    LOG(LS_INFO) << "Mouse lock: " << enable;

    if (!ipc_channel_)
    {
        LOG(LS_WARNING) << "No active IPC channel";
        return;
    }

    proto::internal::ServiceControl* control = outgoing_message_.newMessage().mutable_control();

    control->set_code(proto::internal::ServiceControl::CODE_LOCK_MOUSE);
    control->set_boolean(enable);

    ipc_channel_->send(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::onKeyboardLock(bool enable)
{
    LOG(LS_INFO) << "Keyboard lock: " << enable;

    if (!ipc_channel_)
    {
        LOG(LS_WARNING) << "No active IPC channel";
        return;
    }

    proto::internal::ServiceControl* control = outgoing_message_.newMessage().mutable_control();

    control->set_code(proto::internal::ServiceControl::CODE_LOCK_KEYBOARD);
    control->set_boolean(enable);

    ipc_channel_->send(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::onPause(bool enable)
{
    LOG(LS_INFO) << "Pause: " << enable;

    if (!ipc_channel_)
    {
        LOG(LS_WARNING) << "No active IPC channel";
        return;
    }

    proto::internal::ServiceControl* control = outgoing_message_.newMessage().mutable_control();

    control->set_code(proto::internal::ServiceControl::CODE_PAUSE);
    control->set_boolean(enable);

    ipc_channel_->send(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::onTextChat(const proto::text_chat::TextChat& text_chat)
{
    LOG(LS_INFO) << "Text chat message";

    if (!ipc_channel_)
    {
        LOG(LS_WARNING) << "No active IPC channel";
        return;
    }

    outgoing_message_.newMessage().mutable_text_chat()->CopyFrom(text_chat);
    ipc_channel_->send(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::onIpcDisconnected()
{
    LOG(LS_INFO) << "IPC channel disconncted";
    emit sig_statusChanged(Status::DISCONNECTED_FROM_SERVICE);
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::onIpcMessageReceived(const QByteArray& buffer)
{
    if (!incoming_message_.parse(buffer))
    {
        LOG(LS_ERROR) << "Invalid message from service";
        return;
    }

    if (incoming_message_->has_connect_confirmation_request())
    {
        const proto::internal::ConnectConfirmationRequest& request =
            incoming_message_->connect_confirmation_request();

        LOG(LS_INFO) << "Connect confirmation request received (id=" << request.id()
                     << " computer_name=" << request.computer_name() << " user_name="
                     << request.user_name() << ")";

        emit sig_connectConfirmationRequest(request);
    }
    else if (incoming_message_->has_connect_event())
    {
        LOG(LS_INFO) << "Connect event received";

        clients_.push_back(Client(incoming_message_->connect_event()));
        emit sig_clientListChanged(clients_);
    }
    else if (incoming_message_->has_disconnect_event())
    {
        LOG(LS_INFO) << "Disconnect event received";

        for (auto it = clients_.begin(), it_end = clients_.end(); it != it_end; ++it)
        {
            if (it->id == incoming_message_->disconnect_event().id())
            {
                clients_.erase(it);
                break;
            }
        }

        emit sig_clientListChanged(clients_);
    }
    else if (incoming_message_->has_credentials())
    {
        const proto::internal::Credentials& credentials = incoming_message_->credentials();
        LOG(LS_INFO) << "Credentials received (host_id=" << credentials.host_id() << ")";
        emit sig_credentialsChanged(credentials);
    }
    else if (incoming_message_->has_router_state())
    {
        const proto::internal::RouterState router_state = incoming_message_->router_state();
        LOG(LS_INFO) << "Router state received (" << router_state << ")";
        emit sig_routerStateChanged(router_state);
    }
    else if (incoming_message_->has_text_chat())
    {
        emit sig_textChat(incoming_message_->text_chat());
    }
    else if (incoming_message_->has_video_recording_state())
    {
        const proto::internal::VideoRecordingState& video_recording_state =
            incoming_message_->video_recording_state();

        LOG(LS_INFO) << "Video recording state changed (" << video_recording_state  << ")";

        emit sig_videoRecordingStateChanged(
            QString::fromStdString(video_recording_state.computer_name()),
            QString::fromStdString(video_recording_state.user_name()),
            video_recording_state.started());
    }
    else
    {
        LOG(LS_ERROR) << "Unhandled message from service";
    }
}

} // namespace host
