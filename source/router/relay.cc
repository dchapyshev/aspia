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

#include "router/relay.h"

#include "router/service.h"

namespace {

//--------------------------------------------------------------------------------------------------
qint64 createRelayId()
{
    static qint64 last_relay_id = 0;
    ++last_relay_id;
    return last_relay_id;
}

} // namespace

//--------------------------------------------------------------------------------------------------
Relay::Relay(TcpChannel* channel, QObject* parent)
    : QObject(parent),
      session_id_(createRelayId()),
      tcp_channel_(channel)
{
    CDCHECK(tcp_channel_);
    tcp_channel_->setParent(this);
    address_.setAddress(tcp_channel_->peerAddress());

    connect(tcp_channel_, &TcpChannel::sig_errorOccurred, this, &Relay::onTcpErrorOccurred);
    connect(tcp_channel_, &TcpChannel::sig_messageReceived, this, &Relay::onTcpMessageReceived);

    CLOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
Relay::~Relay()
{
    CLOG(INFO) << "Dtor";
    Service::instance()->removeKeysForRelay(sessionId());
    Service::instance()->notifyChanged(Service::NOTIFY_RELAYS);
}

//--------------------------------------------------------------------------------------------------
void Relay::start()
{
    std::chrono::time_point<std::chrono::system_clock> time_point = std::chrono::system_clock::now();
    start_time_ = std::chrono::system_clock::to_time_t(time_point);
    tcp_channel_->setPaused(false);
    emit sig_started(session_id_);

    Service::instance()->notifyChanged(Service::NOTIFY_RELAYS);
}

//--------------------------------------------------------------------------------------------------
QVersionNumber Relay::version() const
{
    return tcp_channel_->peerVersion();
}

//--------------------------------------------------------------------------------------------------
QString Relay::osName() const
{
    return tcp_channel_->peerOsName();
}

//--------------------------------------------------------------------------------------------------
QString Relay::computerName() const
{
    return tcp_channel_->peerComputerName();
}

//--------------------------------------------------------------------------------------------------
QString Relay::architecture() const
{
    return tcp_channel_->peerArchitecture();
}

//--------------------------------------------------------------------------------------------------
void Relay::sendMessage(quint8 channel_id, const QByteArray& message)
{
    tcp_channel_->send(channel_id, message);
}

//--------------------------------------------------------------------------------------------------
void Relay::sendKeyUsed(quint32 key_id)
{
    outgoing_message_.newMessage().mutable_key_used()->set_key_id(key_id);
    sendMessage(0, outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void Relay::disconnectPeerSession(const proto::router::PeerRequest& request)
{
    outgoing_message_.newMessage().mutable_peer_request()->CopyFrom(request);
    sendMessage(0, outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void Relay::onTcpErrorOccurred(TcpChannel::ErrorCode error_code)
{
    CLOG(INFO) << "Network error:" << error_code;
    emit sig_finished(session_id_);
}

//--------------------------------------------------------------------------------------------------
void Relay::onTcpMessageReceived(quint8 /* channel_id */, const QByteArray& buffer)
{
    if (!incoming_message_.parse(buffer))
    {
        CLOG(ERROR) << "Could not read message from relay server";
        return;
    }

    if (incoming_message_->has_key_pool())
    {
        readKeyPool(incoming_message_->key_pool());
    }
    else if (incoming_message_->has_statistics())
    {
        statistics_ = std::move(*incoming_message_->mutable_statistics());
    }
    else
    {
        CLOG(ERROR) << "Unhandled message from relay server";
    }
}

//--------------------------------------------------------------------------------------------------
void Relay::readKeyPool(const proto::router::RelayKeyPool& key_pool)
{
    Service* service = Service::instance();

    CLOG(INFO) << "Received key pool:" << key_pool.key_size() << "(" << address() << ")";

    peer_data_.emplace(std::make_pair(
        key_pool.peer_host(), static_cast<quint16>(key_pool.peer_port())));

    for (int i = 0; i < key_pool.key_size(); ++i)
        service->addKey(sessionId(), key_pool.key(i));
}
