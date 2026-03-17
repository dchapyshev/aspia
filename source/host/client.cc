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
void Client::send(quint8 channel_id, const QByteArray& buffer)
{
    if (udp_ready_)
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
void Client::onUdpReady()
{
    LOG(INFO) << "UDP channel is connected";
    CHECK(udp_channel_);
    udp_ready_ = true;
    udp_channel_->setPaused(false);
}

//--------------------------------------------------------------------------------------------------
void Client::onUdpErrorOccurred()
{
    LOG(WARNING) << "UDP channel is disconnected";
    CHECK(udp_channel_);
    udp_ready_ = false;
    udp_channel_->disconnect();
    udp_channel_->deleteLater();
    udp_channel_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void Client::onUdpMessageReceived(quint8 udp_channel_id, const QByteArray& buffer)
{
    onMessage(udp_channel_id, buffer);
}

//--------------------------------------------------------------------------------------------------
void Client::readDirectUdpRequest(const proto::peer::DirectUdpRequest& request)
{
    QStringList address_list;

    for (int i = 0; i < request.address_size(); ++i)
    {
        QString address = QString::fromStdString(request.address(i));
        if (address.isEmpty() || !base::NetUtils::isValidIpAddress(address))
            continue;
        address_list.emplace_back(address);
    }

    if (address_list.isEmpty())
    {
        LOG(ERROR) << "No valid addresses";
        return;
    }

    quint32 port = request.port();
    if (!base::NetUtils::isValidPort(port))
    {
        LOG(ERROR) << "Invalid port:" << port;
        return;
    }

    QByteArray public_key = QByteArray::fromStdString(request.public_key());
    if (public_key.isEmpty())
    {
        LOG(ERROR) << "Empty public key";
        return;
    }

    QByteArray iv = QByteArray::fromStdString(request.iv());
    if (iv.isEmpty())
    {
        LOG(ERROR) << "Empty IV";
        return;
    }

    QString address;

    // If there is more than one address, then this is an attempt at a direct connection
    // (without using STUN)
    if (address_list.size() > 1)
    {
        QStringList host_address_list = base::NetUtils::localIpList();

        for (const auto& host_address : std::as_const(host_address_list))
        {
            QHostAddress host_addr(host_address);
            if (host_addr.protocol() != QAbstractSocket::IPv4Protocol)
                continue;

            quint32 host = host_addr.toIPv4Address();
            quint32 host_subnet = host & 0xFFFFFF00; // /24 mask

            bool is_found = false;

            for (const auto& client_address : std::as_const(address_list))
            {
                QHostAddress client_addr(client_address);
                if (client_addr.protocol() != QAbstractSocket::IPv4Protocol)
                    continue;

                quint32 client = client_addr.toIPv4Address();
                quint32 client_subnet = client & 0xFFFFFF00; // /24 mask

                if (client_subnet != host_subnet)
                    continue;

                LOG(INFO) << "Common subnet was found for the host and client";
                address = client_address;
                is_found = true;
                break;
            }

            if (is_found)
                break;
        }
    }
    else
    {
        address = address_list.first();
    }

    if (address.isEmpty())
    {
        LOG(WARNING) << "No suitable address was found for direct connection";
        return;
    }

    LOG(INFO) << "Client UDP endpoint:" << address << ':' << port;
    connectToUdp(address, static_cast<quint16>(port), request.encryptions(), public_key, iv);
}

//--------------------------------------------------------------------------------------------------
void Client::connectToUdp(const QString& address, quint16 port, quint32 encryptions,
    const QByteArray& client_public_key, const QByteArray& client_iv)
{
    if (udp_channel_)
    {
        udp_channel_->disconnect();
        udp_channel_->deleteLater();
        udp_channel_ = nullptr;
        udp_ready_ = false;
    }

    base::KeyPair host_key_pair = base::KeyPair::create(base::KeyPair::Type::X25519);
    if (!host_key_pair.isValid())
    {
        LOG(ERROR) << "Failed to create host UDP key pair";
        return;
    }

    QByteArray host_iv = base::Random::byteArray(12);
    if (host_iv.isEmpty())
    {
        LOG(ERROR) << "Unable to create IV for UDP";
        return;
    }

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

    udp_channel_ = new base::UdpChannel(this);
    udp_channel_->setEncryptor(std::move(encryptor));
    udp_channel_->setDecryptor(std::move(decryptor));

    connect(udp_channel_, &base::UdpChannel::sig_ready, this, &Client::onUdpReady);
    connect(udp_channel_, &base::UdpChannel::sig_errorOccurred, this, &Client::onUdpErrorOccurred);
    connect(udp_channel_, &base::UdpChannel::sig_messageReceived, this, &Client::onUdpMessageReceived);

    udp_channel_->connectTo(address, port);

    // Send reply with host's public key and IV.
    proto::peer::HostToClient reply;
    proto::peer::DirectUdpReply* udp_reply = reply.mutable_direct_udp_reply();
    udp_reply->set_encryption(encryption);
    udp_reply->set_public_key(host_key_pair.publicKey().toStdString());
    udp_reply->set_iv(host_iv.toStdString());
    tcp_channel_->send(proto::peer::CHANNEL_ID_CONTROL, base::serialize(reply));
}

} // namespace host
