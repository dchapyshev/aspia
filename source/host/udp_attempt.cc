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

#include "host/udp_attempt.h"

#include <QStringList>
#include <QTimer>

#include "base/serialization.h"
#include "base/crypto/datagram_decryptor.h"
#include "base/crypto/datagram_encryptor.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_byte_array.h"
#include "base/net/gateway_port_mapper.h"
#include "base/net/net_utils.h"
#include "base/net/udp_channel.h"
#include "base/peer/stun_peer.h"
#include "proto/peer.h"

namespace {

const int kAttemptTimeoutMs = 15000;           // Max time for one attempt to connect before it is dropped.
const qint64 kInitialProbeDataSize = 4 * 1024; // 4 KB - smallest probe, used for the initial measurement.

//--------------------------------------------------------------------------------------------------
QByteArray makeBandwidthProbeData()
{
    std::string payload;
    payload.resize(kInitialProbeDataSize);
    for (size_t i = 0; i < payload.size(); ++i)
        payload[i] = static_cast<char>(i & 0xFF);

    proto::peer::HostToClient message;
    message.mutable_bandwidth_probe()->set_payload(std::move(payload));
    return serialize(message);
}

} // namespace

//--------------------------------------------------------------------------------------------------
UdpAttempt::UdpAttempt(quint32 request_id, QObject* parent)
    : QObject(parent),
      request_id_(request_id),
      key_pair_(KeyPair::create(KeyPair::Type::X25519)),
      iv_(Random::byteArray(12))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
UdpAttempt::~UdpAttempt() = default;

//--------------------------------------------------------------------------------------------------
bool UdpAttempt::isValid() const
{
    return key_pair_.isValid() && !iv_.isEmpty();
}

//--------------------------------------------------------------------------------------------------
ScopedQPointer<UdpChannel> UdpAttempt::takeChannel()
{
    // Stop routing the channel's signals to this attempt; the new owner wires its own handlers.
    if (channel_)
        channel_->disconnect(this);
    return std::move(channel_);
}

//--------------------------------------------------------------------------------------------------
UdpChannel* UdpAttempt::createChannel()
{
    CCHECK(!channel_);

    channel_ = new UdpChannel(this);

    connect(channel_, &UdpChannel::sig_ready, this, &UdpAttempt::onChannelReady);
    connect(channel_, &UdpChannel::sig_errorOccurred, this, [this]() { emit sig_failed(request_id_); });
    connect(channel_, &UdpChannel::sig_messageReceived, this, &UdpAttempt::onChannelMessage);

    return channel_;
}

//--------------------------------------------------------------------------------------------------
bool UdpAttempt::applyCrypto(const proto::peer::UdpReply& reply)
{
    if (!channel_)
        return false;

    std::unique_ptr<DatagramEncryptor> encryptor;
    std::unique_ptr<DatagramDecryptor> decryptor;
    if (!deriveCrypto(reply, &encryptor, &decryptor))
        return false;

    channel_->setEncryptor(std::move(encryptor));
    channel_->setDecryptor(std::move(decryptor));

    crypto_ready_ = true;
    sendProbeIfReady();
    return true;
}

//--------------------------------------------------------------------------------------------------
void UdpAttempt::sendMessage(const proto::peer::HostToClient& message)
{
    emit sig_message(serialize(message));
}

//--------------------------------------------------------------------------------------------------
void UdpAttempt::startTimeout()
{
    QTimer::singleShot(kAttemptTimeoutMs, this, [this]()
    {
        if (connected_)
            return;

        CLOG(WARNING) << "UDP attempt" << request_id_ << "timed out";
        emit sig_failed(request_id_);
    });
}

//--------------------------------------------------------------------------------------------------
bool UdpAttempt::deriveCrypto(const proto::peer::UdpReply& reply,
                              std::unique_ptr<DatagramEncryptor>* encryptor,
                              std::unique_ptr<DatagramDecryptor>* decryptor)
{
    SecureByteArray session_key(key_pair_.sessionKey(QByteArray::fromStdString(reply.public_key())));
    if (session_key.isEmpty())
    {
        CLOG(ERROR) << "Failed to derive UDP session key";
        return false;
    }

    if (reply.encryption() != proto::key_exchange::ENCRYPTION_AES256_GCM)
    {
        CLOG(ERROR) << "Unknown UDP encryption type:" << reply.encryption();
        return false;
    }

    *encryptor = DatagramEncryptor::createForAes256Gcm(session_key, iv_);
    *decryptor = DatagramDecryptor::createForAes256Gcm(
        session_key, QByteArray::fromStdString(reply.iv()));
    if (!*encryptor || !*decryptor)
    {
        CLOG(ERROR) << "Failed to create UDP encryptor/decryptor";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void UdpAttempt::onChannelReady()
{
    // The ENet path is up; allow incoming data and probe the channel once crypto is in place.
    channel_->setPaused(false);
    enet_ready_ = true;
    sendProbeIfReady();
}

//--------------------------------------------------------------------------------------------------
void UdpAttempt::onChannelMessage(quint8 channel_id, const QByteArray& buffer)
{
    if (connected_)
        return;

    if (channel_id != proto::peer::CHANNEL_ID_CONTROL)
    {
        CLOG(ERROR) << "Unexpected message from channel" << channel_id;
        return;
    }

    proto::peer::ClientToHost message;
    if (!parse(buffer, &message))
    {
        CLOG(ERROR) << "Unable to parse UDP control message";
        return;
    }

    if (!message.has_bandwidth_probe_ack())
    {
        CLOG(ERROR) << "Unexpected message";
        return;
    }

    // The peer acked our probe: the channel is confirmed and timed. Report the bandwidth implied by
    // the round-trip of the known-size probe.
    auto rtt = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - probe_send_time_);
    qint64 rtt_ms = rtt.count() > 0 ? rtt.count() : 1;

    connected_ = true;
    emit sig_connected(request_id_, (kInitialProbeDataSize * 1000) / rtt_ms);
}

//--------------------------------------------------------------------------------------------------
void UdpAttempt::sendProbeIfReady()
{
    // Once the channel can carry encrypted data both ways, send a real bandwidth probe and wait for
    // the peer's ack (see onChannelMessage). The round-trip both confirms the channel and yields its
    // initial bandwidth. Both conditions are required to send.
    if (!enet_ready_ || !crypto_ready_ || probe_sent_)
        return;
    probe_sent_ = true;

    probe_send_time_ = std::chrono::steady_clock::now();
    channel_->send(proto::peer::CHANNEL_ID_CONTROL, makeBandwidthProbeData(), true);

    CLOG(INFO) << "UDP attempt" << request_id_ << "sent probe, waiting for ack";
}

//--------------------------------------------------------------------------------------------------
DirectUdpAttempt::DirectUdpAttempt(quint32 request_id, QObject* parent)
    : UdpAttempt(request_id, parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void DirectUdpAttempt::start()
{
    // Passive: bind a local port and advertise the host's local addresses; the client connects.
    createChannel();

    quint16 port = 0;
    channel_->bind(&port);

    proto::peer::HostToClient message;
    proto::peer::DirectUdpRequest* request = message.mutable_direct_udp_request();
    fillKeyExchange(request);

    const QStringList local_ip_list = NetUtils::localIpList();
    for (const auto& local_ip : std::as_const(local_ip_list))
        request->add_address(local_ip.toStdString());

    request->set_port(port);

    CLOG(INFO) << "Direct LAN attempt" << request_id_ << "sending request";
    sendMessage(message);
    startTimeout();
}

//--------------------------------------------------------------------------------------------------
void DirectUdpAttempt::onReply(const proto::peer::UdpReply& reply)
{
    if (!applyCrypto(reply))
        emit sig_failed(request_id_);
}

//--------------------------------------------------------------------------------------------------
StunUdpAttempt::StunUdpAttempt(quint32 request_id, const QString& stun_host, quint16 stun_port,
                               QObject* parent)
    : UdpAttempt(request_id, parent),
      stun_host_(stun_host),
      stun_port_(stun_port)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
StunUdpAttempt::~StunUdpAttempt() = default;

//--------------------------------------------------------------------------------------------------
void StunUdpAttempt::start()
{
    stun_peer_ = new StunPeer(this);

    connect(stun_peer_, &StunPeer::sig_channelReady, this,
        [this](const QString& external_address, quint16 external_port)
    {
        if (!stun_peer_)
            return;

        bool is_white_ip = false;
        const QStringList local_ip_list = NetUtils::localIpList();
        for (const auto& local_ip : std::as_const(local_ip_list))
        {
            if (NetUtils::isAddressEqual(external_address, local_ip))
            {
                is_white_ip = true;
                break;
            }
        }

        qintptr socket = -1;
        quint16 port = 0;
        if (!is_white_ip)
        {
            // Reuse the STUN socket so the discovered external port stays mapped.
            socket = stun_peer_->takeSocket();
            port = external_port;
        }

        stun_peer_->disconnect();
        stun_peer_.reset();

        // Passive: bind the socket and advertise the external endpoint; the client connects to us.
        createChannel();
        if (socket != -1)
            channel_->bind(socket);
        else
            channel_->bind(&port);

        proto::peer::HostToClient message;
        proto::peer::StunUdpRequest* request = message.mutable_stun_udp_request();
        fillKeyExchange(request);
        request->add_address(external_address.toStdString());
        request->set_port(port);
        request->set_stun_host(stun_host_.toStdString());
        request->set_stun_port(stun_port_);

        CLOG(INFO) << "STUN attempt" << request_id_ << "sending request (endpoint" << external_address
                   << ':' << port << ")";
        sendMessage(message);
    });
    connect(stun_peer_, &StunPeer::sig_errorOccurred, this, [this]()
    {
        CLOG(ERROR) << "STUN error (attempt" << request_id_ << ")";
        emit sig_failed(request_id_);
    });

    stun_peer_->start(stun_host_, stun_port_);
    startTimeout();
}

//--------------------------------------------------------------------------------------------------
void StunUdpAttempt::onReply(const proto::peer::UdpReply& reply)
{
    // Punch toward the client's external endpoint so our NAT lets its packets through. Done before
    // crypto so the probe sent from applyCrypto targets the right peer.
    QString client_address = QString::fromStdString(reply.address());
    quint16 client_port = static_cast<quint16>(reply.port());
    if (channel_ && !client_address.isEmpty() && client_port)
    {
        CLOG(INFO) << "STUN attempt" << request_id_ << "punching to" << client_address << ':'
                   << client_port;
        channel_->setPeerAddress(client_address, client_port);
    }

    if (!applyCrypto(reply))
        emit sig_failed(request_id_);
}

//--------------------------------------------------------------------------------------------------
HostGatewayUdpAttempt::HostGatewayUdpAttempt(quint32 request_id, QObject* parent)
    : UdpAttempt(request_id, parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void HostGatewayUdpAttempt::start()
{
    // Passive: bind a local port, map it on the gateway, advertise the mapped endpoint; client connects.
    createChannel();

    quint16 local_port = 0;
    channel_->bind(&local_port);

    // Parent the mapper to the channel so the mapping survives as long as the channel (it travels
    // with the channel if this attempt wins, and is removed with it otherwise).
    GatewayPortMapper* mapper = new GatewayPortMapper(channel_);

    connect(mapper, &GatewayPortMapper::sig_ready, this,
        [this](const QString& external_address, quint16 external_port)
    {
        proto::peer::HostToClient message;
        proto::peer::DirectUdpRequest* request = message.mutable_direct_udp_request();
        fillKeyExchange(request);
        request->add_address(external_address.toStdString());
        request->set_port(external_port);
        request->set_gateway(true);

        CLOG(INFO) << "Host gateway attempt" << request_id_ << "sending request (endpoint"
                   << external_address << ':' << external_port << ")";
        sendMessage(message);
    });
    connect(mapper, &GatewayPortMapper::sig_failed, this, [this]()
    {
        CLOG(WARNING) << "Host gateway mapping failed (attempt" << request_id_ << ")";
        emit sig_failed(request_id_);
    });

    mapper->addUdpMapping(local_port);
    startTimeout();
}

//--------------------------------------------------------------------------------------------------
void HostGatewayUdpAttempt::onReply(const proto::peer::UdpReply& reply)
{
    if (!applyCrypto(reply))
        emit sig_failed(request_id_);
}

//--------------------------------------------------------------------------------------------------
ClientGatewayUdpAttempt::ClientGatewayUdpAttempt(quint32 request_id, QObject* parent)
    : UdpAttempt(request_id, parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void ClientGatewayUdpAttempt::start()
{
    // Active: ask the client to open a mapping and report its endpoint; the channel is created in
    // onReply once that endpoint arrives.
    proto::peer::HostToClient message;
    proto::peer::GatewayUdpRequest* request = message.mutable_gateway_udp_request();
    fillKeyExchange(request);

    CLOG(INFO) << "Client gateway attempt" << request_id_ << "sending request";
    sendMessage(message);
    startTimeout();
}

//--------------------------------------------------------------------------------------------------
void ClientGatewayUdpAttempt::onReply(const proto::peer::UdpReply& reply)
{
    QString client_address = QString::fromStdString(reply.address());
    quint16 client_port = static_cast<quint16>(reply.port());
    if (client_address.isEmpty() || !client_port)
    {
        CLOG(WARNING) << "Client gateway endpoint unavailable (attempt" << request_id_ << ")";
        emit sig_failed(request_id_);
        return;
    }

    createChannel();
    if (!applyCrypto(reply))
    {
        emit sig_failed(request_id_);
        return;
    }

    CLOG(INFO) << "Client gateway attempt" << request_id_ << "connecting to" << client_address << ':'
               << client_port;
    channel_->connectTo(client_address, client_port);
}
