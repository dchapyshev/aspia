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

#include "host/client_session.h"

#include "base/logging.h"
#include "base/serialization.h"
#include "host/client_session_desktop.h"
#include "host/client_session_file_transfer.h"
#include "host/client_session_port_forwarding.h"
#include "host/client_session_system_info.h"
#include "host/client_session_text_chat.h"

namespace host {

//--------------------------------------------------------------------------------------------------
ClientSession::ClientSession(
    proto::SessionType session_type, std::unique_ptr<base::TcpChannel> channel, QObject* parent)
    : QObject(parent),
      session_type_(session_type),
      channel_(std::move(channel))
{
    DCHECK(channel_);

    // All sessions are executed in one thread. We can safely use a global counter to get session IDs.
    // Session IDs must start with 1.
    static quint32 id_counter = 0;
    id_ = ++id_counter;

    LOG(LS_INFO) << "Ctor (id=" << id_ << ")";
}

ClientSession::~ClientSession()
{
    LOG(LS_INFO) << "Dtor (id=" << id_ << ")";
}

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<ClientSession> ClientSession::create(proto::SessionType session_type,
                                                     std::unique_ptr<base::TcpChannel> channel,
                                                     QObject* parent)
{
    if (!channel)
    {
        LOG(LS_ERROR) << "Invalid network channel";
        return nullptr;
    }

    switch (session_type)
    {
        case proto::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::SESSION_TYPE_DESKTOP_VIEW:
            return std::unique_ptr<ClientSessionDesktop>(
                new ClientSessionDesktop(session_type, std::move(channel), parent));

        case proto::SESSION_TYPE_FILE_TRANSFER:
            return std::unique_ptr<ClientSessionFileTransfer>(
                new ClientSessionFileTransfer(std::move(channel), parent));

        case proto::SESSION_TYPE_SYSTEM_INFO:
            return std::unique_ptr<ClientSessionSystemInfo>(
                new ClientSessionSystemInfo(std::move(channel), parent));

        case proto::SESSION_TYPE_TEXT_CHAT:
            return std::unique_ptr<ClientSessionTextChat>(
                new ClientSessionTextChat(std::move(channel), parent));

        case proto::SESSION_TYPE_PORT_FORWARDING:
            return std::unique_ptr<ClientSessionPortForwarding>(
                new ClientSessionPortForwarding(std::move(channel), parent));

        default:
            LOG(LS_ERROR) << "Unknown session type: " << session_type;
            return nullptr;
    }
}

//--------------------------------------------------------------------------------------------------
void ClientSession::start()
{
    LOG(LS_INFO) << "Starting client session";
    state_ = State::STARTED;

    connect(channel_.get(), &base::TcpChannel::sig_disconnected,
            this, &ClientSession::onTcpDisconnected);
    connect(channel_.get(), &base::TcpChannel::sig_messageReceived,
            this, &ClientSession::onTcpMessageReceived);
    connect(channel_.get(), &base::TcpChannel::sig_messageWritten,
            this, &ClientSession::onTcpMessageWritten);

    channel_->setKeepAlive(true);
    channel_->resume();

    onStarted();
}

//--------------------------------------------------------------------------------------------------
void ClientSession::stop()
{
    LOG(LS_INFO) << "Stop client session";

    state_ = State::FINISHED;
    emit sig_clientSessionFinished();
}

//--------------------------------------------------------------------------------------------------
void ClientSession::setClientVersion(const QVersionNumber& version)
{
    version_ = version;
}

//--------------------------------------------------------------------------------------------------
void ClientSession::setUserName(const QString& username)
{
    username_ = username;
}

//--------------------------------------------------------------------------------------------------
void ClientSession::setComputerName(const QString& computer_name)
{
    computer_name_ = computer_name;
}

//--------------------------------------------------------------------------------------------------
const QString& ClientSession::computerName() const
{
    return computer_name_;
}

//--------------------------------------------------------------------------------------------------
void ClientSession::setDisplayName(const QString& display_name)
{
    display_name_ = display_name;
}

//--------------------------------------------------------------------------------------------------
const QString& ClientSession::displayName() const
{
    return display_name_;
}

//--------------------------------------------------------------------------------------------------
void ClientSession::setSessionId(base::SessionId session_id)
{
    session_id_ = session_id;
}

//--------------------------------------------------------------------------------------------------
void ClientSession::sendMessage(quint8 channel_id, QByteArray&& buffer)
{
    channel_->send(channel_id, std::move(buffer));
}

//--------------------------------------------------------------------------------------------------
void ClientSession::sendMessage(quint8 channel_id, const google::protobuf::MessageLite& message)
{
    channel_->send(channel_id, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
size_t ClientSession::pendingMessages() const
{
    return channel_->pendingMessages();
}

//--------------------------------------------------------------------------------------------------
void ClientSession::onTcpDisconnected(base::NetworkChannel::ErrorCode error_code)
{
    LOG(LS_ERROR) << "Client disconnected with error: "
                  << base::NetworkChannel::errorToString(error_code);

    state_ = State::FINISHED;
    emit sig_clientSessionFinished();
}

//--------------------------------------------------------------------------------------------------
void ClientSession::onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer)
{
    if (channel_id == proto::HOST_CHANNEL_ID_SESSION)
    {
        onReceived(channel_id, buffer);
    }
    else if (channel_id == proto::HOST_CHANNEL_ID_SERVICE)
    {
        // TODO
    }
    else
    {
        LOG(LS_ERROR) << "Unhandled incoming message from channel: " << channel_id;
    }
}

//--------------------------------------------------------------------------------------------------
void ClientSession::onTcpMessageWritten(quint8 channel_id, size_t pending)
{
    if (channel_id == proto::HOST_CHANNEL_ID_SESSION)
    {
        onWritten(channel_id, pending);
    }
    else if (channel_id == proto::HOST_CHANNEL_ID_SERVICE)
    {
        // TODO
    }
    else
    {
        LOG(LS_ERROR) << "Unhandled outgoing message from channel: " << channel_id;
    }
}

} // namespace host
