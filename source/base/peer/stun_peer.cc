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

#include "base/peer/stun_peer.h"

#include <QHostInfo>
#include <QSocketNotifier>
#include <QTimer>

#include <enet/enet.h>

#include "base/location.h"
#include "base/serialization.h"
#include "base/crypto/random.h"
#include "base/net/net_utils.h"
#include "proto/stun.h"

namespace base {

//--------------------------------------------------------------------------------------------------
StunPeer::StunPeer(QObject* parent)
    : QObject(parent),
      timer_(new QTimer(this)),
      read_notifier_(new QSocketNotifier(QSocketNotifier::Read, this))
{
    CLOG(INFO) << "Ctor";
    timer_->setSingleShot(true);
    connect(timer_, &QTimer::timeout, this, &StunPeer::onAttempt);
    connect(read_notifier_, &QSocketNotifier::activated, this, &StunPeer::onReadyRead);
}

//--------------------------------------------------------------------------------------------------
StunPeer::~StunPeer()
{
    CLOG(INFO) << "Dtor";
    doStop();

    if (ready_socket_ != -1)
    {
        enet_socket_destroy(ready_socket_);
        ready_socket_ = -1;
    }
}

//--------------------------------------------------------------------------------------------------
void StunPeer::start(const QString& stun_host, quint16 stun_port)
{
    stun_host_ = stun_host;
    stun_port_ = stun_port;
    doStart();
}

//--------------------------------------------------------------------------------------------------
qintptr StunPeer::takeSocket()
{
    qintptr socket = ready_socket_;
    ready_socket_ = -1;
    return socket;
}

//--------------------------------------------------------------------------------------------------
void StunPeer::onAttempt()
{
    static const int kMaxAttemptsCount = 3;

    if (number_of_attempts_ >= kMaxAttemptsCount)
    {
        onErrorOccurred(FROM_HERE);
        return;
    }

    doStop();
    doStart();
}

//--------------------------------------------------------------------------------------------------
void StunPeer::doStart()
{
    ++number_of_attempts_;

    CLOG(INFO) << "Stun peer starting (" << stun_host_ << ":" << stun_port_
               << ", attempt" << number_of_attempts_ << ")";

    timer_->start(std::chrono::seconds(3));
    lookup_id_ = QHostInfo::lookupHost(stun_host_, this, &StunPeer::onHostResolved);
}

//--------------------------------------------------------------------------------------------------
void StunPeer::doStop()
{
    timer_->stop();

    if (lookup_id_ != -1)
    {
        QHostInfo::abortHostLookup(lookup_id_);
        lookup_id_ = -1;
    }

    read_notifier_->setEnabled(false);

    if (socket_ != -1)
    {
        enet_socket_destroy(socket_);
        socket_ = -1;
    }
}

//--------------------------------------------------------------------------------------------------
void StunPeer::onHostResolved(const QHostInfo& host_info)
{
    lookup_id_ = -1;

    if (host_info.error() != QHostInfo::NoError)
    {
        CLOG(ERROR) << "DNS lookup failed:" << host_info.errorString();
        onErrorOccurred(FROM_HERE);
        return;
    }

    const QList<QHostAddress>& addresses = host_info.addresses();
    if (addresses.isEmpty())
    {
        onErrorOccurred(FROM_HERE);
        return;
    }

    // Find the first IPv4 address.
    QHostAddress selected;
    for (const QHostAddress& addr : addresses)
    {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol)
        {
            selected = addr;
            break;
        }
    }

    if (selected.isNull())
    {
        CLOG(ERROR) << "No IPv4 address found for" << stun_host_;
        onErrorOccurred(FROM_HERE);
        return;
    }

    CLOG(INFO) << "Resolved" << stun_host_ << "to" << selected;

    // Create raw UDP socket.
    socket_ = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
    if (socket_ == ENET_SOCKET_NULL)
    {
        onErrorOccurred(FROM_HERE);
        return;
    }

    if (enet_socket_set_option(socket_, ENET_SOCKOPT_NONBLOCK, 1) != 0)
    {
        onErrorOccurred(FROM_HERE);
        return;
    }

    // Bind to any available port. Do NOT connect() the socket - a connected UDP socket can only
    // send/receive to/from the connected peer, and enet needs the socket unconnected to communicate
    // with arbitrary addresses.
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = 0;

    if (enet_socket_bind(socket_, &address) != 0)
    {
        onErrorOccurred(FROM_HERE);
        return;
    }

    // Remember the resolved address for sendto().
    stun_address_ = selected;

    CLOG(INFO) << "Bound UDP socket, STUN server:" << selected << ":" << stun_port_;

    // Send the STUN request.
    transaction_id_ = Random::number32();

    proto::stun::PeerToStun message;
    proto::stun::EndpointRequest* request = message.mutable_endpoint_request();
    request->set_magic_number(0xA0B1C2D3);
    request->set_transaction_id(transaction_id_);

    QByteArray buffer = base::serialize(message);
    if (buffer.isEmpty())
    {
        onErrorOccurred(FROM_HERE);
        return;
    }

    ENetAddress enet_address;
    enet_address.port = stun_port_;

    if (enet_address_set_host(&enet_address, stun_address_.toString().toLocal8Bit().constData()) != 0)
    {
        onErrorOccurred(FROM_HERE);
        return;
    }

    ENetBuffer enet_buffer;
    enet_buffer.data = const_cast<char*>(buffer.constData());
    enet_buffer.dataLength = static_cast<size_t>(buffer.size());

    int sent = enet_socket_send(socket_, &enet_address, &enet_buffer, 1);
    if (sent != buffer.size())
    {
        onErrorOccurred(FROM_HERE);
        return;
    }

    // Set up read notification.
    read_notifier_->setSocket(socket_);
    read_notifier_->setEnabled(true);
}

//--------------------------------------------------------------------------------------------------
void StunPeer::onReadyRead()
{
    static const int kMaxRecvSize = 1024;
    char recv_buffer[kMaxRecvSize];

    ENetBuffer enet_buffer;
    enet_buffer.data = recv_buffer;
    enet_buffer.dataLength = static_cast<size_t>(kMaxRecvSize);

    int received = enet_socket_receive(socket_, nullptr, &enet_buffer, 1);
    if (received <= 0)
    {
        CLOG(ERROR) << "Failed to receive STUN response";
        onErrorOccurred(FROM_HERE);
        return;
    }

    proto::stun::StunToPeer message;
    if (!base::parse(QByteArray(recv_buffer, received), &message))
    {
        onErrorOccurred(FROM_HERE);
        return;
    }

    if (!message.has_endpoint())
    {
        onErrorOccurred(FROM_HERE);
        return;
    }

    const proto::stun::Endpoint& endpoint = message.endpoint();
    if (endpoint.transaction_id() != transaction_id_)
    {
        onErrorOccurred(FROM_HERE);
        return;
    }

    QString ip_address = QString::fromStdString(endpoint.ip_address());
    quint32 port = static_cast<quint32>(endpoint.port());

    CLOG(INFO) << "External endpoint:" << ip_address << ":" << port;

    if (!NetUtils::isValidIpAddress(ip_address))
    {
        onErrorOccurred(FROM_HERE);
        return;
    }

    if (!NetUtils::isValidPort(port))
    {
        onErrorOccurred(FROM_HERE);
        return;
    }

    // Transfer ownership of the raw socket.
    ready_socket_ = socket_;
    socket_ = -1;

    read_notifier_->setEnabled(false);
    timer_->stop();

    emit sig_channelReady(ip_address, static_cast<quint16>(port));
}

//--------------------------------------------------------------------------------------------------
void StunPeer::onErrorOccurred(const Location& location)
{
    CLOG(ERROR) << "STUN error occurred" << location;
    timer_->stop();
    emit sig_errorOccurred();
}

} // namespace base
