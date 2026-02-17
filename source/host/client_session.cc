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

#include "host/client_session.h"

#include "base/logging.h"
#include "host/client_session_desktop.h"
#include "host/client_session_file_transfer.h"
#include "host/client_session_system_info.h"
#include "host/client_session_text_chat.h"

namespace host {

//--------------------------------------------------------------------------------------------------
ClientSession::ClientSession(base::TcpChannel* channel, QObject* parent)
    : QObject(parent),
      tcp_channel_(channel)
{
    DCHECK(tcp_channel_);

    tcp_channel_->setParent(this);

    // All sessions are executed in one thread. We can safely use a global counter to get session IDs.
    // Session IDs must start with 1.
    static quint32 id_counter = 0;
    id_ = ++id_counter;

    LOG(INFO) << "Ctor (id=" << id_ << ")";
}

ClientSession::~ClientSession()
{
    LOG(INFO) << "Dtor (id=" << id_ << ")";
}

//--------------------------------------------------------------------------------------------------
// static
ClientSession* ClientSession::create(base::TcpChannel* channel, QObject* parent)
{
    if (!channel)
    {
        LOG(ERROR) << "Invalid network channel";
        return nullptr;
    }

    switch (channel->peerSessionType())
    {
        case proto::peer::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::peer::SESSION_TYPE_DESKTOP_VIEW:
            return new ClientSessionDesktop(channel, parent);

        case proto::peer::SESSION_TYPE_FILE_TRANSFER:
            return new ClientSessionFileTransfer(channel, parent);

        case proto::peer::SESSION_TYPE_SYSTEM_INFO:
            return new ClientSessionSystemInfo(channel, parent);

        case proto::peer::SESSION_TYPE_TEXT_CHAT:
            return new ClientSessionTextChat(channel, parent);

        default:
        {
            LOG(ERROR) << "Unknown session type:" << channel->peerSessionType();
            channel->deleteLater();
            return nullptr;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void ClientSession::start()
{
    LOG(INFO) << "Starting client session";
    state_ = State::STARTED;

    connect(tcp_channel_, &base::TcpChannel::sig_errorOccurred, this, &ClientSession::onTcpErrorOccurred);
    connect(tcp_channel_, &base::TcpChannel::sig_messageReceived, this, &ClientSession::onTcpMessageReceived);

    tcp_channel_->resume();
    onStarted();
}

//--------------------------------------------------------------------------------------------------
void ClientSession::stop()
{
    LOG(INFO) << "Stop client session";

    state_ = State::FINISHED;
    emit sig_clientSessionFinished();
}

//--------------------------------------------------------------------------------------------------
QVersionNumber ClientSession::clientVersion() const
{
    if (!tcp_channel_)
    {
        LOG(ERROR) << "TCP channel is unavailable";
        return QVersionNumber();
    }

    return tcp_channel_->peerVersion();
}

//--------------------------------------------------------------------------------------------------
QString ClientSession::userName() const
{
    if (!tcp_channel_)
    {
        LOG(ERROR) << "TCP channel is unavailable";
        return QString();
    }

    return tcp_channel_->peerUserName();
}

//--------------------------------------------------------------------------------------------------
QString ClientSession::computerName() const
{
    if (!tcp_channel_)
    {
        LOG(ERROR) << "TCP channel is unavailable";
        return QString();
    }

    return tcp_channel_->peerComputerName();
}

//--------------------------------------------------------------------------------------------------
QString ClientSession::displayName() const
{
    if (!tcp_channel_)
    {
        LOG(ERROR) << "TCP channel is unavailable";
        return QString();
    }

    return tcp_channel_->peerDisplayName();
}

//--------------------------------------------------------------------------------------------------
proto::peer::SessionType ClientSession::sessionType() const
{
    if (!tcp_channel_)
    {
        LOG(ERROR) << "TCP channel is unavailable";
        return proto::peer::SESSION_TYPE_UNKNOWN;
    }

    return static_cast<proto::peer::SessionType>(tcp_channel_->peerSessionType());
}

//--------------------------------------------------------------------------------------------------
void ClientSession::setSessionId(base::SessionId session_id)
{
    session_id_ = session_id;
}

//--------------------------------------------------------------------------------------------------
base::HostId ClientSession::hostId() const
{
    return tcp_channel_->hostId();
}

//--------------------------------------------------------------------------------------------------
void ClientSession::sendMessage(const QByteArray& buffer)
{
    tcp_channel_->send(proto::peer::CHANNEL_ID_SESSION, buffer);
}

//--------------------------------------------------------------------------------------------------
size_t ClientSession::pendingMessages() const
{
    return tcp_channel_->pendingMessages();
}

//--------------------------------------------------------------------------------------------------
void ClientSession::onTcpErrorOccurred(base::TcpChannel::ErrorCode error_code)
{
    LOG(ERROR) << "Client disconnected with error:" << error_code;
    state_ = State::FINISHED;
    emit sig_clientSessionFinished();
}

//--------------------------------------------------------------------------------------------------
void ClientSession::onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer)
{
    if (channel_id == proto::peer::CHANNEL_ID_SESSION)
    {
        onReceived(buffer);
    }
    else if (channel_id == proto::peer::CHANNEL_ID_SERVICE)
    {
        // TODO
    }
    else
    {
        LOG(ERROR) << "Unhandled incoming message from channel:" << channel_id;
    }
}

} // namespace host
