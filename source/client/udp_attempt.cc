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

#include "client/udp_attempt.h"

#include <QHostAddress>
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
#include "proto/key_exchange.h"
#include "proto/peer.h"

namespace {

const int kAttemptTimeoutMs = 15000; // Max time for one attempt to win before it is dropped.

} // namespace

//--------------------------------------------------------------------------------------------------
UdpAttempt::UdpAttempt(quint32 request_id, const QByteArray& host_public_key,
                       const QByteArray& host_iv, quint32 encryptions, QObject* parent)
    : QObject(parent),
      request_id_(request_id),
      host_public_key_(host_public_key),
      host_iv_(host_iv),
      encryptions_(encryptions),
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
    return key_pair_.isValid() && !iv_.isEmpty() && !host_public_key_.isEmpty() &&
           !host_iv_.isEmpty();
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

    if (!(encryptions_ & proto::key_exchange::ENCRYPTION_AES256_GCM))
    {
        CLOG(ERROR) << "No supported UDP encryption";
        return nullptr;
    }

    SecureByteArray session_key(key_pair_.sessionKey(host_public_key_));
    if (session_key.isEmpty())
    {
        CLOG(ERROR) << "Failed to derive UDP session key";
        return nullptr;
    }

    std::unique_ptr<DatagramEncryptor> encryptor =
        DatagramEncryptor::createForAes256Gcm(session_key, iv_);
    std::unique_ptr<DatagramDecryptor> decryptor =
        DatagramDecryptor::createForAes256Gcm(session_key, host_iv_);
    if (!encryptor || !decryptor)
    {
        CLOG(ERROR) << "Failed to create UDP encryptor/decryptor";
        return nullptr;
    }

    channel_ = new UdpChannel(this);
    channel_->setEncryptor(std::move(encryptor));
    channel_->setDecryptor(std::move(decryptor));

    connect(channel_, &UdpChannel::sig_ready, this, [this]() { channel_->setPaused(false); });
    connect(channel_, &UdpChannel::sig_errorOccurred, this, [this]() { emit sig_failed(request_id_); });
    connect(channel_, &UdpChannel::sig_messageReceived, this, &UdpAttempt::onChannelMessage);

    return channel_;
}

//--------------------------------------------------------------------------------------------------
void UdpAttempt::onChannelMessage(quint8 channel_id, const QByteArray& buffer)
{
    if (connected_)
        return;

    // The winner is decided by the host's probe; nothing else is expected before that.
    if (channel_id != proto::peer::CHANNEL_ID_CONTROL)
    {
        CLOG(ERROR) << "Unexpected message from channel" << channel_id;
        return;
    }

    proto::peer::HostToClient message;
    if (!parse(buffer, &message))
    {
        CLOG(ERROR) << "Unable to parse UDP control message";
        return;
    }

    if (!message.has_bandwidth_probe())
    {
        CLOG(ERROR) << "Unexpected message";
        return;
    }

    // The host probes the channel it selected: this attempt wins. Acknowledge the probe and report
    // success; the owner then takes the channel.
    proto::peer::ClientToHost ack;
    ack.mutable_bandwidth_probe_ack()->set_dummy(1);
    channel_->send(proto::peer::CHANNEL_ID_CONTROL, serialize(ack), true);

    connected_ = true;
    emit sig_connected(request_id_);
}

//--------------------------------------------------------------------------------------------------
void UdpAttempt::sendReply(const QString& external_address, quint16 external_port)
{
    proto::peer::ClientToHost message;
    proto::peer::UdpReply* reply = message.mutable_udp_reply();
    reply->set_request_id(request_id_);
    reply->set_encryption(proto::key_exchange::ENCRYPTION_AES256_GCM);
    reply->set_public_key(key_pair_.publicKey().toStdString());
    reply->set_iv(iv_.toStdString());

    if (!external_address.isEmpty() && external_port)
    {
        reply->set_address(external_address.toStdString());
        reply->set_port(external_port);
    }

    emit sig_message(serialize(message));
}

//--------------------------------------------------------------------------------------------------
void UdpAttempt::startTimeout()
{
    // If this attempt has not won by the deadline it is dropped. A won attempt is deleted by the
    // owner before the deadline, which cancels this timer.
    QTimer::singleShot(kAttemptTimeoutMs, this, [this]()
    {
        if (connected_)
            return;

        CLOG(WARNING) << "UDP attempt" << request_id_ << "timed out";
        emit sig_failed(request_id_);
    });
}

//--------------------------------------------------------------------------------------------------
DirectUdpAttempt::DirectUdpAttempt(quint32 request_id, const QByteArray& host_public_key,
                                   const QByteArray& host_iv, quint32 encryptions,
                                   const QStringList& addresses, quint16 port, QObject* parent)
    : UdpAttempt(request_id, host_public_key, host_iv, encryptions, parent),
      addresses_(addresses),
      port_(port)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void DirectUdpAttempt::start()
{
    QString address;

    // With more than one address pick the one sharing a /24 subnet with a local interface (LAN);
    // otherwise use the single advertised address.
    if (addresses_.size() > 1)
    {
        const QStringList local_ip_list = NetUtils::localIpList();
        for (const auto& local_ip : std::as_const(local_ip_list))
        {
            QHostAddress local_address(local_ip);
            if (local_address.protocol() != QAbstractSocket::IPv4Protocol)
                continue;

            const quint32 local_subnet = local_address.toIPv4Address() & 0xFFFFFF00; // /24 mask

            bool is_found = false;
            for (const auto& host_address : std::as_const(addresses_))
            {
                QHostAddress candidate(host_address);
                if (candidate.protocol() != QAbstractSocket::IPv4Protocol)
                    continue;

                if (local_subnet != (candidate.toIPv4Address() & 0xFFFFFF00))
                    continue;

                CLOG(INFO) << "Common subnet was found for the host and client";
                address = host_address;
                is_found = true;
                break;
            }

            if (is_found)
                break;
        }
    }
    else if (!addresses_.isEmpty())
    {
        address = addresses_.first();
    }

    if (address.isEmpty())
    {
        CLOG(WARNING) << "No suitable address for direct connection (attempt" << request_id_ << ")";
        emit sig_failed(request_id_);
        return;
    }

    if (!createChannel())
    {
        emit sig_failed(request_id_);
        return;
    }

    CLOG(INFO) << "Direct attempt" << request_id_ << "connecting to" << address << ':' << port_;
    channel_->connectTo(address, port_);
    sendReply();
    startTimeout();
}

//--------------------------------------------------------------------------------------------------
StunUdpAttempt::StunUdpAttempt(quint32 request_id, const QByteArray& host_public_key,
                               const QByteArray& host_iv, quint32 encryptions,
                               const QString& host_address, quint16 host_port,
                               const QString& stun_host, quint16 stun_port, QObject* parent)
    : UdpAttempt(request_id, host_public_key, host_iv, encryptions, parent),
      host_address_(host_address),
      host_port_(host_port),
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

        // Reuse the STUN socket so this client's discovered external port stays mapped.
        qintptr socket = stun_peer_->takeSocket();

        stun_peer_->disconnect();
        stun_peer_.reset();

        if (!createChannel())
        {
            emit sig_failed(request_id_);
            return;
        }

        CLOG(INFO) << "STUN attempt" << request_id_ << "connecting to host" << host_address_ << ':'
                   << host_port_ << "(local external" << external_address << ':' << external_port << ")";
        channel_->connectTo(socket, host_address_, host_port_);

        // Report our external endpoint so the host can punch toward it.
        sendReply(external_address, external_port);
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
GatewayUdpAttempt::GatewayUdpAttempt(quint32 request_id, const QByteArray& host_public_key,
                                     const QByteArray& host_iv, quint32 encryptions, QObject* parent)
    : UdpAttempt(request_id, host_public_key, host_iv, encryptions, parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void GatewayUdpAttempt::start()
{
    // Passive: listen on an OS-assigned local port and map it on the gateway; the host connects to it.
    if (!createChannel())
    {
        emit sig_failed(request_id_);
        return;
    }

    quint16 local_port = 0;
    channel_->bind(&local_port);

    // The mapper is owned by the channel so the mapping lives as long as the channel.
    GatewayPortMapper* mapper = new GatewayPortMapper(channel_);

    connect(mapper, &GatewayPortMapper::sig_ready, this, &GatewayUdpAttempt::sendReply);
    connect(mapper, &GatewayPortMapper::sig_failed, this, [this]()
    {
        CLOG(WARNING) << "Client gateway mapping failed (attempt" << request_id_ << ")";
        emit sig_failed(request_id_);
    });

    mapper->addUdpMapping(local_port);
    startTimeout();
}
