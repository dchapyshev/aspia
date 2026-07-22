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

#include "host/workers/user_ipc_worker.h"

#include "base/logging.h"
#include "base/numeric_utils.h"
#include "base/ipc/ipc_channel.h"
#include "common/clipboard_file_transfer.h"
#include "host/host_constants.h"
#include "proto/desktop_channel.h"
#include "proto/desktop_clipboard.h"
#include "proto/desktop_user.h"

namespace {

volatile auto g_statusType = qRegisterMetaType<UserIpcWorker::Status>();
volatile auto g_clientListType = qRegisterMetaType<UserIpcWorker::ClientList>();

} // namespace

//--------------------------------------------------------------------------------------------------
UserIpcWorker::UserIpcWorker()
    : Worker(Thread::AsioDispatcher)
{
    LOG(INFO) << "Ctor";
#if defined(Q_OS_WINDOWS)
    // 0x100-0x1FF Application reserved last shutdown range.
    if (!SetProcessShutdownParameters(0x100, SHUTDOWN_NORETRY))
        PLOG(ERROR) << "SetProcessShutdownParameters failed";
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
UserIpcWorker::~UserIpcWorker()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void UserIpcWorker::onConnectToService()
{
    LOG(INFO) << "Starting user session agent (channel:" << kHostUiChannelId << ")";

    ipc_channel_ = new IpcChannel(this);

    connect(ipc_channel_, &IpcChannel::sig_connected, this, &UserIpcWorker::onIpcConnected);
    connect(ipc_channel_, &IpcChannel::sig_disconnected, this, &UserIpcWorker::onIpcDisconnected);
    connect(ipc_channel_, &IpcChannel::sig_errorOccurred, this, &UserIpcWorker::onIpcErrorOccurred);
    connect(ipc_channel_, &IpcChannel::sig_messageReceived, this, &UserIpcWorker::onIpcMessageReceived);

    ipc_channel_->connectTo(kHostUiChannelId);
}

//--------------------------------------------------------------------------------------------------
void UserIpcWorker::onUpdateCredentials(proto::user::CredentialsRequest_Type type)
{
    LOG(INFO) << "Update credentials request:" << type;
    proto::user::CredentialsRequest* request =
        outgoing_message_.newMessage<proto::user::UserToService>().mutable_credentials_request();
    request->set_type(type);
    sendServiceMessage();
}

//--------------------------------------------------------------------------------------------------
void UserIpcWorker::onOneTimeSessions(quint32 sessions)
{
    LOG(INFO) << "One-time sessions changed:" << sessions;
    proto::user::OneTimeSessions* one_time_sessions =
        outgoing_message_.newMessage<proto::user::UserToService>().mutable_one_time_sessions();
    one_time_sessions->set_sessions(sessions);
    sendServiceMessage();
}

//--------------------------------------------------------------------------------------------------
void UserIpcWorker::onStopClient(quint32 client_id)
{
    LOG(INFO) << "Stop client request:" << client_id;
    proto::user::ServiceControl* control =
        outgoing_message_.newMessage<proto::user::UserToService>().mutable_control();
    control->set_command_name("stop_client");
    control->set_unsigned_integer(client_id);
    sendServiceMessage();
}

//--------------------------------------------------------------------------------------------------
void UserIpcWorker::onConnectConfirmation(quint32 id, bool accept)
{
    LOG(INFO) << "Connect confirmation (id:" << id << "accept:" << accept << ")";
    proto::user::ConfirmationReply* confirmation =
        outgoing_message_.newMessage<proto::user::UserToService>().mutable_confirmation_reply();
    confirmation->set_id(id);
    confirmation->set_accept(accept);
    sendServiceMessage();
}

//--------------------------------------------------------------------------------------------------
void UserIpcWorker::onMouseLock(bool enable)
{
    LOG(INFO) << "Mouse lock:" << enable;
    proto::user::ServiceControl* control =
        outgoing_message_.newMessage<proto::user::UserToService>().mutable_control();
    control->set_command_name("lock_mouse");
    control->set_boolean(enable);
    sendServiceMessage();
}

//--------------------------------------------------------------------------------------------------
void UserIpcWorker::onKeyboardLock(bool enable)
{
    LOG(INFO) << "Keyboard lock:" << enable;
    proto::user::ServiceControl* control =
        outgoing_message_.newMessage<proto::user::UserToService>().mutable_control();
    control->set_command_name("lock_keyboard");
    control->set_boolean(enable);
    sendServiceMessage();
}

//--------------------------------------------------------------------------------------------------
void UserIpcWorker::onPause(bool enable)
{
    LOG(INFO) << "Pause:" << enable;
    proto::user::ServiceControl* control =
        outgoing_message_.newMessage<proto::user::UserToService>().mutable_control();
    control->set_command_name("pause");
    control->set_boolean(enable);
    sendServiceMessage();
}

//--------------------------------------------------------------------------------------------------
void UserIpcWorker::onChat(const proto::chat::Chat& chat)
{
    LOG(INFO) << "Text chat message";
    outgoing_message_.newMessage<proto::user::UserToService>().mutable_chat()->CopyFrom(chat);
    sendServiceMessage();
}

//--------------------------------------------------------------------------------------------------
void UserIpcWorker::onClipboardEvent(const proto::clipboard::Event& event)
{
    if (!ipc_channel_)
        return;

    proto::clipboard::HostToClient message;
    message.mutable_event()->CopyFrom(event);
    sendNetworkMessage(proto::desktop::CHANNEL_ID_CLIPBOARD, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void UserIpcWorker::onClipboardLocalFileListChanged(const QVector<LocalFileEntry>& files)
{
    if (clipboard_file_transfer_)
        clipboard_file_transfer_->setLocalFileList(files);
}

//--------------------------------------------------------------------------------------------------
void UserIpcWorker::onClipboardFileDataRequest(int file_index)
{
    if (clipboard_file_transfer_)
        clipboard_file_transfer_->requestFileData(file_index);
}

//--------------------------------------------------------------------------------------------------
void UserIpcWorker::onStart()
{
    LOG(INFO) << "User IPC worker started";
}

//--------------------------------------------------------------------------------------------------
void UserIpcWorker::onStop()
{
    LOG(INFO) << "User IPC worker stopped";

    clipboard_file_transfer_.reset();

    if (ipc_channel_)
    {
        ipc_channel_->disconnect(this);
        ipc_channel_.reset();
    }

    clients_.clear();
}

//--------------------------------------------------------------------------------------------------
void UserIpcWorker::onIpcConnected()
{
    LOG(INFO) << "IPC channel connected";
    emit sig_statusChanged(Status::CONNECTED_TO_SERVICE);
    ipc_channel_->setPaused(false);
}

//--------------------------------------------------------------------------------------------------
void UserIpcWorker::onIpcDisconnected()
{
    LOG(INFO) << "IPC channel disconncted";

    if (ipc_channel_)
    {
        ipc_channel_->disconnect(this);
        ipc_channel_.reset();
    }

    emit sig_statusChanged(Status::DISCONNECTED_FROM_SERVICE);
}

//--------------------------------------------------------------------------------------------------
void UserIpcWorker::onIpcErrorOccurred()
{
    LOG(INFO) << "Unable to connect to IPC server";

    if (ipc_channel_)
    {
        ipc_channel_->disconnect(this);
        ipc_channel_.reset();
    }

    emit sig_statusChanged(Status::SERVICE_NOT_AVAILABLE);
}

//--------------------------------------------------------------------------------------------------
void UserIpcWorker::onIpcMessageReceived(quint32 channel_id, const QByteArray& buffer, bool /* reliable */)
{
    quint16 net_channel_id = lowWord(channel_id);
    quint16 ipc_channel_id = highWord(channel_id);

    if (ipc_channel_id == proto::user::CHANNEL_ID_NETWORK)
    {
        if (net_channel_id == proto::desktop::CHANNEL_ID_CLIPBOARD)
        {
            proto::clipboard::ClientToHost message;
            if (!parse(buffer, &message))
            {
                LOG(ERROR) << "Unable to parse clipboard message";
                return;
            }

            if (message.has_event())
            {
                emit sig_injectClipboardEvent(message.event());
            }
            else
            {
                LOG(ERROR) << "Unhandled clipboard message";
            }
        }
        else if (net_channel_id == proto::desktop::CHANNEL_ID_USER)
        {
            proto::user::ClientToHost message;
            if (!parse(buffer, &message))
            {
                LOG(ERROR) << "Unable to parse user message";
                return;
            }

            if (message.has_video_recording())
            {
                bool started =
                    message.video_recording().action() == proto::user::VideoRecording::ACTION_STARTED;
                LOG(INFO) << "Video recording state changed:" << started;
                emit sig_recordingStateChanged(started);
            }
        }
        else if (net_channel_id == proto::desktop::CHANNEL_ID_FILE)
        {
            if (clipboard_file_transfer_)
                clipboard_file_transfer_->onIncomingMessage(buffer);
        }
        return;
    }

    proto::user::ServiceToUser* message =
        incoming_message_.parse<proto::user::ServiceToUser>(buffer);
    if (!message)
    {
        LOG(ERROR) << "Invalid message from service";
        return;
    }

    if (message->has_confirmation_request())
    {
        const proto::user::ConfirmationRequest& request = message->confirmation_request();

        LOG(INFO) << "Connect confirmation request received (id:" << request.id()
                  << "computer:" << request.computer_name() << "user:" << request.user_name() << ")";

        emit sig_confirmationRequest(request);
    }
    else if (message->has_connect_event())
    {
        onConnectEvent(message->connect_event());
    }
    else if (message->has_disconnect_event())
    {
        onDisconnectEvent(message->disconnect_event());
    }
    else if (message->has_credentials())
    {
        const proto::user::Credentials& credentials = message->credentials();
        LOG(INFO) << "Credentials received (host_id" << credentials.host_id() << ")";
        emit sig_credentialsChanged(credentials);
    }
    else if (message->has_router_state())
    {
        const proto::user::RouterState router_state = message->router_state();
        LOG(INFO) << "Router state received (" << router_state << ")";
        emit sig_routerStateChanged(router_state);
    }
    else if (message->has_chat())
    {
        emit sig_chat(message->chat());
    }
    else
    {
        LOG(ERROR) << "Unhandled message from service";
    }
}

//--------------------------------------------------------------------------------------------------
void UserIpcWorker::onConnectEvent(const proto::user::ConnectEvent& event)
{
    LOG(INFO) << "Connect event received";
    clients_.emplace_back(event);

    if (!clipboard_file_transfer_)
    {
        // The clipboard itself lives on the GUI thread (created by HostWindow); only the file
        // transfer, which sends/receives over the network, stays here.
        clipboard_file_transfer_ = new ClipboardFileTransfer(this);

        connect(clipboard_file_transfer_, &ClipboardFileTransfer::sig_sendMessage,
                this, [this](const QByteArray& buffer)
        {
            sendNetworkMessage(proto::desktop::CHANNEL_ID_FILE, buffer);
        });

        connect(clipboard_file_transfer_, &ClipboardFileTransfer::sig_fileDataChunk,
                this, &UserIpcWorker::sig_clipboardFileData);
    }

    emit sig_clientListChanged(clients_);
}

//--------------------------------------------------------------------------------------------------
void UserIpcWorker::onDisconnectEvent(const proto::user::DisconnectEvent& event)
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

    bool has_desktop = false;

    for (const auto& client : std::as_const(clients_))
    {
        if (client.session_type == proto::peer::SESSION_TYPE_DESKTOP)
            has_desktop = true;
    }

    if (!has_desktop)
    {
        LOG(INFO) << "Last desktop client is disconnected";

        clipboard_file_transfer_.reset();
    }

    emit sig_clientListChanged(clients_);
}

//--------------------------------------------------------------------------------------------------
void UserIpcWorker::sendServiceMessage()
{
    if (!ipc_channel_)
        return;

    quint32 channel_id = makeUint32(proto::user::CHANNEL_ID_SERVICE, 0);
    ipc_channel_->send(channel_id, outgoing_message_.serialize<proto::user::UserToService>());
}

//--------------------------------------------------------------------------------------------------
void UserIpcWorker::sendNetworkMessage(quint8 net_channel_id, const QByteArray& buffer)
{
    if (!ipc_channel_)
        return;

    quint32 channel_id = makeUint32(proto::user::CHANNEL_ID_NETWORK, net_channel_id);
    ipc_channel_->send(channel_id, buffer);
}
