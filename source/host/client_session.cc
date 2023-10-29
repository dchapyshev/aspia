//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "base/net/tcp_channel_proxy.h"
#include "host/client_session_desktop.h"
#include "host/client_session_file_transfer.h"
#include "host/client_session_system_info.h"
#include "host/client_session_text_chat.h"

namespace host {

//--------------------------------------------------------------------------------------------------
ClientSession::ClientSession(
    proto::SessionType session_type, std::unique_ptr<base::TcpChannel> channel)
    : session_type_(session_type),
      channel_(std::move(channel))
{
    DCHECK(channel_);

    // All sessions are executed in one thread. We can safely use a global counter to get session IDs.
    // Session IDs must start with 1.
    static uint32_t id_counter = 0;
    id_ = ++id_counter;

    LOG(LS_INFO) << "Ctor: " << id_;
}

//--------------------------------------------------------------------------------------------------
ClientSession::~ClientSession()
{
    LOG(LS_INFO) << "Dtor: " << id_;
}

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<ClientSession> ClientSession::create(proto::SessionType session_type,
                                                     std::unique_ptr<base::TcpChannel> channel,
                                                     std::shared_ptr<base::TaskRunner> task_runner)
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
                new ClientSessionDesktop(session_type, std::move(channel), std::move(task_runner)));

        case proto::SESSION_TYPE_FILE_TRANSFER:
            return std::unique_ptr<ClientSessionFileTransfer>(
                new ClientSessionFileTransfer(std::move(channel), std::move(task_runner)));

        case proto::SESSION_TYPE_SYSTEM_INFO:
            return std::unique_ptr<ClientSessionSystemInfo>(
                new ClientSessionSystemInfo(std::move(channel)));

        case proto::SESSION_TYPE_TEXT_CHAT:
            return std::unique_ptr<ClientSessionTextChat>(
                new ClientSessionTextChat(std::move(channel)));

        default:
            LOG(LS_ERROR) << "Unknown session type: " << session_type;
            return nullptr;
    }
}

//--------------------------------------------------------------------------------------------------
void ClientSession::start(Delegate* delegate)
{
    LOG(LS_INFO) << "Starting client session";
    state_ = State::STARTED;

    delegate_ = delegate;
    DCHECK(delegate_);

    channel_->setListener(this);
    channel_->setKeepAlive(true);
    channel_->resume();

    onStarted();
}

//--------------------------------------------------------------------------------------------------
void ClientSession::stop()
{
    LOG(LS_INFO) << "Stop client session";

    state_ = State::FINISHED;
    delegate_->onClientSessionFinished();
}

//--------------------------------------------------------------------------------------------------
void ClientSession::setVersion(const base::Version& version)
{
    version_ = version;
}

//--------------------------------------------------------------------------------------------------
void ClientSession::setUserName(std::string_view username)
{
    username_ = username;
}

//--------------------------------------------------------------------------------------------------
void ClientSession::setComputerName(std::string_view computer_name)
{
    computer_name_ = computer_name;
}

//--------------------------------------------------------------------------------------------------
std::string ClientSession::computerName() const
{
    return computer_name_;
}

//--------------------------------------------------------------------------------------------------
void ClientSession::setSessionId(base::SessionId session_id)
{
    session_id_ = session_id;
}

//--------------------------------------------------------------------------------------------------
std::shared_ptr<base::TcpChannelProxy> ClientSession::channelProxy()
{
    return channel_->channelProxy();
}

//--------------------------------------------------------------------------------------------------
void ClientSession::sendMessage(uint8_t channel_id, base::ByteArray&& buffer)
{
    channel_->send(channel_id, std::move(buffer));
}

//--------------------------------------------------------------------------------------------------
void ClientSession::onTcpConnected()
{
    NOTREACHED();
}

//--------------------------------------------------------------------------------------------------
void ClientSession::onTcpDisconnected(base::NetworkChannel::ErrorCode error_code)
{
    LOG(LS_ERROR) << "Client disconnected with error: "
                  << base::NetworkChannel::errorToString(error_code);

    state_ = State::FINISHED;
    delegate_->onClientSessionFinished();
}

//--------------------------------------------------------------------------------------------------
void ClientSession::onTcpMessageReceived(uint8_t channel_id, const base::ByteArray& buffer)
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
void ClientSession::onTcpMessageWritten(uint8_t channel_id, size_t pending)
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

//--------------------------------------------------------------------------------------------------
size_t ClientSession::pendingMessages() const
{
    return channel_->pendingMessages();
}

} // namespace host
