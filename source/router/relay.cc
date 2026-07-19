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

#include "router/shared_key_pool.h"

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

    connect(tcp_channel_, &TcpChannel::sig_errorOccurred, this, &Relay::onTcpErrorOccurred);
    connect(tcp_channel_, &TcpChannel::sig_messageReceived, this, &Relay::onTcpMessageReceived);

    CLOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
Relay::~Relay()
{
    CLOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void Relay::start()
{
    std::chrono::time_point<std::chrono::system_clock> time_point = std::chrono::system_clock::now();
    start_time_ = std::chrono::system_clock::to_time_t(time_point);
    tcp_channel_->setPaused(false);
    emit sig_started(session_id_);
}

//--------------------------------------------------------------------------------------------------
QVersionNumber Relay::version() const
{
    return tcp_channel_->peerVersion();
}

//--------------------------------------------------------------------------------------------------
const std::string& Relay::osName() const
{
    return tcp_channel_->peerOsName();
}

//--------------------------------------------------------------------------------------------------
const std::string& Relay::computerName() const
{
    return tcp_channel_->peerComputerName();
}

//--------------------------------------------------------------------------------------------------
const std::string& Relay::architecture() const
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
    outgoing_message_.newMessage<proto::router::RouterToRelay>().mutable_key_used()->set_key_id(key_id);
    sendMessage(0, outgoing_message_.serialize<proto::router::RouterToRelay>());
}

//--------------------------------------------------------------------------------------------------
void Relay::disconnectPeerSession(const proto::router::PeerRequest& request)
{
    outgoing_message_.newMessage<proto::router::RouterToRelay>().mutable_peer_request()->CopyFrom(request);
    sendMessage(0, outgoing_message_.serialize<proto::router::RouterToRelay>());
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
    proto::router::RelayToRouter* message =
        incoming_message_.parse<proto::router::RelayToRouter>(buffer);
    if (!message)
    {
        CLOG(ERROR) << "Could not read message from relay server";
        return;
    }

    if (message->has_key_pool())
    {
        readKeyPool(message->key_pool());
    }
    else if (message->has_statistics())
    {
        statistics_ = std::move(*message->mutable_statistics());
    }
    else
    {
        CLOG(ERROR) << "Unhandled message from relay server";
    }
}

//--------------------------------------------------------------------------------------------------
void Relay::readKeyPool(const proto::router::RelayKeyPool& key_pool)
{
    CLOG(INFO) << "Received key pool:" << key_pool.key_size() << "(" << address() << ")";

    const std::string& peer_host = key_pool.peer_host();
    const quint32 peer_port = key_pool.peer_port();

    // The endpoint comes from the relay over the wire; reject a pool with an empty host or an
    // out-of-range port instead of storing a truncated quint16 that would yield a broken offer.
    if (peer_host.empty() || peer_port == 0 || peer_port > 65535)
    {
        CLOG(ERROR) << "Ignoring key pool with invalid peer endpoint (host:" << peer_host
                    << "port:" << peer_port << ")";
        return;
    }

    for (int i = 0; i < key_pool.key_size(); ++i)
    {
        SharedKeyPool::instance().add(
            session_id_, peer_host, static_cast<quint16>(peer_port), key_pool.key(i));
    }
}
