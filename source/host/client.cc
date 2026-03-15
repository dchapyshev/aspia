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

#include "host/client.h"

#include <QHostAddress>

#include "base/logging.h"
#include "base/serialization.h"
#include "base/crypto/key_pair.h"
#include "base/crypto/message_decryptor.h"
#include "base/crypto/message_encryptor.h"
#include "base/crypto/random.h"
#include "base/net/net_utils.h"
#include "base/net/udp_channel.h"
#include "base/peer/stun_peer.h"
#include "proto/key_exchange.h"

namespace host {

//--------------------------------------------------------------------------------------------------
Client::Client(base::TcpChannel* tcp_channel, QObject* parent)
    : QObject(parent),
      tcp_channel_(tcp_channel)
{
    LOG(INFO) << "Ctor";
    CHECK(tcp_channel_);

    tcp_channel_->setParent(this);

    connect(tcp_channel_, &base::TcpChannel::sig_errorOccurred, this, &Client::onTcpErrorOccurred);
    connect(tcp_channel_, &base::TcpChannel::sig_messageReceived, this, &Client::onTcpMessageReceived);
    connect(this, &Client::sig_started, this, [this]()
    {
        tcp_channel_->setPaused(false);
        if (udp_channel_)
            udp_channel_->setPaused(false);
    });
}

//--------------------------------------------------------------------------------------------------
Client::~Client()
{
    tcp_channel_->disconnect();
}

//--------------------------------------------------------------------------------------------------
void Client::start()
{
    if (tcp_channel_->type() == base::TcpChannel::Type::RELAY && !stun_host_.isEmpty() && stun_port_)
    {
        stun_peer_ = new base::StunPeer(this);

        connect(stun_peer_, &base::StunPeer::sig_channelReady, this, &Client::onStunChannelReady);
        connect(stun_peer_, &base::StunPeer::sig_errorOccurred, this, &Client::onStunErrorOccurred);

        stun_peer_->start(stun_host_, stun_port_);
    }

    onStart();
}

//--------------------------------------------------------------------------------------------------
quint32 Client::clientId() const
{
    return tcp_channel_->instanceId();
}

//--------------------------------------------------------------------------------------------------
proto::peer::SessionType Client::sessionType() const
{
    return static_cast<proto::peer::SessionType>(tcp_channel_->peerSessionType());
}

//--------------------------------------------------------------------------------------------------
QVersionNumber Client::version() const
{
    return tcp_channel_->peerVersion();
}

//--------------------------------------------------------------------------------------------------
QString Client::osName() const
{
    return tcp_channel_->peerOsName();
}

//--------------------------------------------------------------------------------------------------
QString Client::computerName() const
{
    return tcp_channel_->peerComputerName();
}

//--------------------------------------------------------------------------------------------------
QString Client::displayName() const
{
    return tcp_channel_->peerDisplayName();
}

//--------------------------------------------------------------------------------------------------
QString Client::architecture() const
{
    return tcp_channel_->peerArchitecture();
}

//--------------------------------------------------------------------------------------------------
QString Client::userName() const
{
    return tcp_channel_->peerUserName();
}

//--------------------------------------------------------------------------------------------------
qint64 Client::pendingBytes() const
{
    // TODO: UDP support.
    return tcp_channel_->pendingBytes();
}

//--------------------------------------------------------------------------------------------------
void Client::setStunInfo(const QString& host, quint16 port)
{
    stun_host_ = host;
    stun_port_ = port;
}

//--------------------------------------------------------------------------------------------------
void Client::send(quint8 channel_id, const QByteArray& buffer, bool udp)
{
    if (udp && udp_ready_)
    {
        udp_channel_->send(channel_id, buffer);
        return;
    }

    tcp_channel_->send(channel_id, buffer);
}

//--------------------------------------------------------------------------------------------------
void Client::onTcpErrorOccurred(base::TcpChannel::ErrorCode error_code)
{
    LOG(ERROR) << "TCP error:" << error_code;
    CHECK(tcp_channel_);
    tcp_channel_->disconnect();
    emit sig_finished();
}

//--------------------------------------------------------------------------------------------------
void Client::onTcpMessageReceived(quint8 tcp_channel_id, const QByteArray& buffer)
{
    if (tcp_channel_id != proto::peer::CHANNEL_ID_CONTROL)
    {
        onMessage(tcp_channel_id, buffer);
        return;
    }

    proto::peer::ClientToHost message;
    if (!base::parse(buffer, &message))
    {
        LOG(ERROR) << "Unable to parse control message";
        return;
    }

    if (message.has_direct_udp_request())
        readDirectUdpRequest(message.direct_udp_request());
    else
        LOG(WARNING) << "Unhandled control message";
}

//--------------------------------------------------------------------------------------------------
void Client::onUdpConnected()
{
    LOG(INFO) << "UDP channel connected";
    CHECK(udp_channel_);
    udp_channel_->setPaused(false);

    udp_test_data_ = base::Random::byteArray(8).toHex();

    LOG(INFO) << "Send UDP test data:" << udp_test_data_;
    udp_channel_->send(proto::peer::CHANNEL_ID_0, udp_test_data_);
}

//--------------------------------------------------------------------------------------------------
void Client::onUdpErrorOccurred()
{
    CHECK(udp_channel_);
    udp_channel_->disconnect();
    udp_channel_->deleteLater();
    udp_channel_ = nullptr;
    udp_test_data_.clear();
    udp_ready_ = false;

    LOG(WARNING) << "UDP channel is disabled";
    emit sig_udpStateChanged(false);
}

//--------------------------------------------------------------------------------------------------
void Client::onUdpMessageReceived(quint8 udp_channel_id, const QByteArray& buffer)
{
    if (!udp_ready_)
    {
        if (buffer != udp_test_data_)
        {
            LOG(ERROR) << "Test data mismatch:" << udp_test_data_ << buffer;

            udp_channel_->disconnect();
            udp_channel_->deleteLater();
            udp_channel_ = nullptr;

            udp_test_data_.clear();
            return;
        }

        udp_test_data_.clear();
        udp_ready_ = true;

        LOG(INFO) << "UDP channel is enabled";
        emit sig_udpStateChanged(true);
        return;
    }

    onMessage(udp_channel_id, buffer);
}

//--------------------------------------------------------------------------------------------------
void Client::onStunChannelReady(const QString& external_address, quint16 /* external_port */)
{
    CHECK(stun_peer_);
    stun_peer_->disconnect();
    stun_peer_->deleteLater();
    stun_peer_ = nullptr;

    LOG(INFO) << "External address for host:" << external_address;
    host_ext_address_ = external_address;
}

//--------------------------------------------------------------------------------------------------
void Client::onStunErrorOccurred()
{
    CHECK(stun_peer_);
    stun_peer_->disconnect();
    stun_peer_->deleteLater();
    stun_peer_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void Client::readDirectUdpRequest(const proto::peer::DirectUdpRequest& request)
{
    if (!request.has_external_endpoint() || !request.local_endpoints_size() || !request.has_credentials())
    {
        LOG(INFO) << "Request does not contain the required data";
        return;
    }

    const proto::peer::DirectUdpRequest::Endpoint& client_ext_endpoint = request.external_endpoint();
    const proto::peer::DirectUdpRequest::Credentials& credentials = request.credentials();

    QString client_ext_address = QString::fromStdString(client_ext_endpoint.ip_address());
    quint32 client_ext_port = client_ext_endpoint.port();
    QByteArray client_public_key = QByteArray::fromStdString(credentials.public_key());
    QByteArray client_iv = QByteArray::fromStdString(credentials.iv());

    LOG(INFO) << "External client UDP endpoint:" << client_ext_address << ':' << client_ext_port;

    // If the external addresses of the client and host are different, then we connect through the
    // hole punched by stun.
    if (!base::NetUtils::isAddressEqual(client_ext_address, host_ext_address_))
    {
        connectToUdp(client_ext_address, client_ext_port, credentials.encryptions(),
            client_public_key, client_iv);
        return;
    }

    LOG(INFO) << "External addresses of the client and host match";

    // If the external addresses for the client and host are the same, then most likely they are
    // located within the same local network. We are trying to find a common subnet for them and
    // connect directly.
    QStringList host_ips = base::NetUtils::localIpList();
    for (const auto& host_ip : std::as_const(host_ips))
    {
        QHostAddress host_addr(host_ip);
        if (host_addr.protocol() != QAbstractSocket::IPv4Protocol)
            continue;

        quint32 host = host_addr.toIPv4Address();
        quint32 host_subnet = host & 0xFFFFFF00; // /24 mask

        for (int i = 0; i < request.local_endpoints_size(); ++i)
        {
            const proto::peer::DirectUdpRequest::Endpoint& client_local_endpoint = request.local_endpoints(i);

            QString client_local_address = QString::fromStdString(client_local_endpoint.ip_address());
            quint32 client_local_port = client_local_endpoint.port();

            QHostAddress client_addr(client_local_address);
            if (client_addr.protocol() != QAbstractSocket::IPv4Protocol)
                continue;

            quint32 client = client_addr.toIPv4Address();
            quint32 client_subnet = client & 0xFFFFFF00; // /24 mask

            if (client_subnet != host_subnet)
                continue;

            LOG(INFO) << "Common subnet was found for the host and client";

            connectToUdp(client_local_address, client_local_port, credentials.encryptions(),
                         client_public_key, client_iv);
            return;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void Client::connectToUdp(const QString& address, quint16 port, quint32 encryptions,
    const QByteArray& client_public_key, const QByteArray& client_iv)
{
    CHECK(!udp_channel_);

    base::KeyPair host_key_pair = base::KeyPair::create(base::KeyPair::Type::X25519);
    if (!host_key_pair.isValid())
    {
        LOG(ERROR) << "Failed to create host UDP key pair";
        return;
    }

    QByteArray host_iv = base::Random::byteArray(12);

    QByteArray session_key = host_key_pair.sessionKey(client_public_key);
    if (session_key.isEmpty())
    {
        LOG(ERROR) << "Failed to derive UDP session key";
        return;
    }

    quint32 encryption = proto::key_exchange::ENCRYPTION_UNKNOWN;
    if (encryptions & proto::key_exchange::ENCRYPTION_AES256_GCM)
        encryption = proto::key_exchange::ENCRYPTION_AES256_GCM;
    else if (encryptions & proto::key_exchange::ENCRYPTION_CHACHA20_POLY1305)
        encryption = proto::key_exchange::ENCRYPTION_CHACHA20_POLY1305;

    if (encryption == proto::key_exchange::ENCRYPTION_UNKNOWN)
    {
        LOG(ERROR) << "No supported UDP encryption";
        return;
    }

    std::unique_ptr<base::MessageEncryptor> encryptor;
    std::unique_ptr<base::MessageDecryptor> decryptor;

    if (encryption == proto::key_exchange::ENCRYPTION_AES256_GCM)
    {
        encryptor = base::MessageEncryptor::createForAes256Gcm(session_key, host_iv);
        decryptor = base::MessageDecryptor::createForAes256Gcm(session_key, client_iv);
    }
    else
    {
        encryptor = base::MessageEncryptor::createForChaCha20Poly1305(session_key, host_iv);
        decryptor = base::MessageDecryptor::createForChaCha20Poly1305(session_key, client_iv);
    }

    if (!encryptor || !decryptor)
    {
        LOG(ERROR) << "Failed to create UDP encryptor/decryptor";
        return;
    }

    // Send reply with host's public key and IV.
    proto::peer::HostToClient reply;
    proto::peer::DirectUdpReply* udp_reply = reply.mutable_direct_udp_reply();
    udp_reply->set_encryption(encryption);
    udp_reply->set_public_key(host_key_pair.publicKey().toStdString());
    udp_reply->set_iv(host_iv.toStdString());
    tcp_channel_->send(proto::peer::CHANNEL_ID_CONTROL, base::serialize(reply));

    udp_channel_ = new base::UdpChannel(this);
    udp_channel_->setEncryptor(std::move(encryptor));
    udp_channel_->setDecryptor(std::move(decryptor));

    connect(udp_channel_, &base::UdpChannel::sig_connected, this, &Client::onUdpConnected);
    connect(udp_channel_, &base::UdpChannel::sig_errorOccurred, this, &Client::onUdpErrorOccurred);
    connect(udp_channel_, &base::UdpChannel::sig_messageReceived, this, &Client::onUdpMessageReceived);

    udp_channel_->connectTo(address, port);
}

} // namespace host
