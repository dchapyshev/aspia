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
#include "base/peer/stun_peer.h"
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
void Client::start(bool direct, const QString& stun_host, quint16 stun_port, bool peer_equals)
{
    direct_ = direct;
    peer_equals_ = peer_equals;
    stun_host_ = stun_host;
    stun_port_ = stun_port;

    onStart();

    if (direct || peer_equals)
    {
        udp_phase_ = UdpConnectPhase::DIRECT_LAN;
        startDirectUdp(-1, QString(), 0);
        return;
    }

    if (stun_host.isEmpty() || !stun_port)
        return;

    udp_phase_ = UdpConnectPhase::HOLE_PUNCHING;
    startUdpHolePunching();
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
    qint64 result = tcp_channel_->pendingBytes();
    if (udp_channel_)
        result += udp_channel_->pendingBytes();

    return result;
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

    if (message.has_direct_udp_reply())
    {
        const proto::peer::DirectUdpReply& udp_reply = message.direct_udp_reply();

        LOG(INFO) << "Direct UDP reply is received";

        if (!udp_channel_ || !udp_key_pair_.isValid())
        {
            LOG(ERROR) << "UDP reply received but no UDP channel or key pair";
            return;
        }

        QByteArray host_public_key = QByteArray::fromStdString(udp_reply.public_key());
        QByteArray host_iv = QByteArray::fromStdString(udp_reply.iv());
        quint32 encryption = udp_reply.encryption();

        QByteArray session_key = udp_key_pair_.sessionKey(host_public_key);
        if (session_key.isEmpty())
        {
            LOG(ERROR) << "Failed to derive UDP session key";
            return;
        }

        std::unique_ptr<base::MessageEncryptor> encryptor;
        std::unique_ptr<base::MessageDecryptor> decryptor;

        if (encryption == proto::key_exchange::ENCRYPTION_AES256_GCM)
        {
            encryptor = base::MessageEncryptor::createForAes256Gcm(session_key, udp_iv_);
            decryptor = base::MessageDecryptor::createForAes256Gcm(session_key, host_iv);
        }
        else if (encryption == proto::key_exchange::ENCRYPTION_CHACHA20_POLY1305)
        {
            encryptor = base::MessageEncryptor::createForChaCha20Poly1305(session_key, udp_iv_);
            decryptor = base::MessageDecryptor::createForChaCha20Poly1305(session_key, host_iv);
        }
        else
        {
            LOG(ERROR) << "Unknown UDP encryption type:" << encryption;
            return;
        }

        if (!encryptor || !decryptor)
        {
            LOG(ERROR) << "Failed to create UDP encryptor/decryptor";
            return;
        }

        udp_channel_->setEncryptor(std::move(encryptor));
        udp_channel_->setDecryptor(std::move(decryptor));

        // Key pair is no longer needed.
        udp_key_pair_ = base::KeyPair();
        udp_iv_.clear();
    }
    else
    {
        LOG(WARNING) << "Unhandled control message";
    }
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
    LOG(INFO) << "UDP channel error (phase:" << udp_phase_ << ")";
    CHECK(udp_channel_);

    bool was_connected = udp_ready_;

    udp_ready_ = false;
    udp_channel_->disconnect();
    udp_channel_->deleteLater();
    udp_channel_ = nullptr;

    // If the UDP was already working and then dropped, we stay on TCP without retrying.
    if (was_connected)
    {
        udp_phase_ = UdpConnectPhase::NONE;
        return;
    }

    // Fallback chain depending on the current phase.
    switch (udp_phase_)
    {
        case UdpConnectPhase::DIRECT_LAN:
        {
            if (direct_)
            {
                udp_phase_ = UdpConnectPhase::NONE;
                return;
            }

            LOG(INFO) << "Direct LAN UDP failed, trying hole punching";
            udp_phase_ = UdpConnectPhase::HOLE_PUNCHING;
            startUdpHolePunching();
        }
        break;

        case UdpConnectPhase::HOLE_PUNCHING:
            LOG(INFO) << "First UDP hole punching attempt failed, retrying";
            udp_phase_ = UdpConnectPhase::HOLE_PUNCHING_RETRY;
            startUdpHolePunching();
            break;

        case UdpConnectPhase::HOLE_PUNCHING_RETRY:
            LOG(INFO) << "All UDP attempts exhausted, staying on TCP";
            udp_phase_ = UdpConnectPhase::NONE;
            break;

        default:
            udp_phase_ = UdpConnectPhase::NONE;
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void Client::onUdpMessageReceived(quint8 udp_channel_id, const QByteArray& buffer)
{
    onMessage(udp_channel_id, buffer);
}

//--------------------------------------------------------------------------------------------------
void Client::startUdpHolePunching()
{
    if (stun_host_.isEmpty() || !stun_port_)
    {
        LOG(ERROR) << "No stun server data";
        return;
    }

    LOG(INFO) << "Stun server data:" << stun_host_ << ':' << stun_port_;
    stun_peer_ = new base::StunPeer(this);

    connect(stun_peer_, &base::StunPeer::sig_channelReady, this,
        [this](const QString& external_address, quint16 external_port)
    {
        LOG(INFO) << "External UDP endpoint received:" << external_address << ':' << external_port
                          << " (phase:" << udp_phase_ << ")";
        CHECK(stun_peer_);

        bool is_white_ip = false;

        // On HOLE_PUNCHING_RETRY we always use the ready socket (previous bind attempt
        // already failed for white IP, or previous ready_socket attempt failed for NAT).
        if (udp_phase_ != UdpConnectPhase::HOLE_PUNCHING_RETRY)
        {
            QStringList local_ip_list = base::NetUtils::localIpList();
            for (const auto& local_ip : std::as_const(local_ip_list))
            {
                if (!base::NetUtils::isAddressEqual(external_address, local_ip))
                    continue;

                // The peer has a white external address (without NAT).
                is_white_ip = true;
                break;
            }
        }

        qintptr socket = -1;
        QString address;
        quint16 port = 0;

        if (!is_white_ip)
        {
            // Use the STUN socket with the discovered external endpoint.
            socket = stun_peer_->takeSocket();
            address = external_address;
            port = external_port;
            LOG(INFO) << "Using ready UDP socket (NAT or forced retry)";
        }
        else
        {
            LOG(INFO) << "White IP detected, using bind for UDP";
        }

        stun_peer_->disconnect();
        stun_peer_->deleteLater();
        stun_peer_ = nullptr;

        startDirectUdp(socket, address, port);
    });

    connect(stun_peer_, &base::StunPeer::sig_errorOccurred, this, [this]()
    {
        LOG(ERROR) << "STUN error (phase:" << udp_phase_ << ")";
        CHECK(stun_peer_);

        stun_peer_->disconnect();
        stun_peer_->deleteLater();
        stun_peer_ = nullptr;

        // STUN failed, advance to next phase.
        if (udp_phase_ == UdpConnectPhase::HOLE_PUNCHING)
        {
            LOG(INFO) << "STUN failed, retrying hole punching";
            udp_phase_ = UdpConnectPhase::HOLE_PUNCHING_RETRY;
            startUdpHolePunching();
        }
        else
        {
            LOG(INFO) << "STUN failed twice, staying on TCP";
            udp_phase_ = UdpConnectPhase::NONE;
        }
    });

    stun_peer_->start(stun_host_, stun_port_);
}

//--------------------------------------------------------------------------------------------------
void Client::startDirectUdp(qintptr socket, const QString& address, quint16 port)
{
    LOG(INFO) << "Starting direct UDP...";
    CHECK(!udp_channel_);

    udp_channel_ = new base::UdpChannel(this);

    connect(udp_channel_, &base::UdpChannel::sig_ready, this, &Client::onUdpReady);
    connect(udp_channel_, &base::UdpChannel::sig_errorOccurred, this, &Client::onUdpErrorOccurred);
    connect(udp_channel_, &base::UdpChannel::sig_messageReceived, this, &Client::onUdpMessageReceived);

    if (socket != -1)
        udp_channel_->setReadySocket(socket);
    else
        udp_channel_->bind(&port);

    udp_key_pair_ = base::KeyPair::create(base::KeyPair::Type::X25519);
    if (!udp_key_pair_.isValid())
    {
        LOG(ERROR) << "Failed to create UDP key pair";
        return;
    }

    udp_iv_ = base::Random::byteArray(12);
    if (udp_iv_.isEmpty())
    {
        LOG(ERROR) << "Unable to create IV for UDP";
        return;
    }

    static const quint32 kSupportedEncryptios =
        proto::key_exchange::ENCRYPTION_AES256_GCM | proto::key_exchange::ENCRYPTION_CHACHA20_POLY1305;

    proto::peer::HostToClient message;
    proto::peer::DirectUdpRequest* request = message.mutable_direct_udp_request();

    if (address.isEmpty())
    {
        QStringList local_ip_list = base::NetUtils::localIpList();
        for (const auto& local_ip : std::as_const(local_ip_list))
            request->add_address(local_ip.toStdString());
    }
    else
    {
        request->add_address(address.toStdString());
    }

    request->set_port(port);
    request->set_encryptions(kSupportedEncryptios);
    request->set_public_key(udp_key_pair_.publicKey().toStdString());
    request->set_iv(udp_iv_.toStdString());

    LOG(INFO) << "Sending direct UDP request";
    tcp_channel_->send(proto::peer::CHANNEL_ID_CONTROL, base::serialize(message));
}

} // namespace host
