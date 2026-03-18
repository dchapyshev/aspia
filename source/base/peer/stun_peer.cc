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

#include <QHostAddress>
#include <QHostInfo>
#include <QSocketNotifier>
#include <QTimer>

#ifdef Q_OS_WIN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

#include "base/location.h"
#include "base/logging.h"
#include "base/serialization.h"
#include "base/crypto/random.h"
#include "base/net/net_utils.h"
#include "proto/stun.h"

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
void closeNativeSocket(qintptr descriptor)
{
#if defined(Q_OS_WINDOWS)
    closesocket(static_cast<SOCKET>(descriptor));
#else
    ::close(static_cast<int>(descriptor));
#endif
}

//--------------------------------------------------------------------------------------------------
qintptr createUdpSocket()
{
#if defined(Q_OS_WINDOWS)
    SOCKET sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET)
        return -1;

    u_long non_blocking = 1;
    if (ioctlsocket(sock, FIONBIO, &non_blocking) != 0)
    {
        closesocket(sock);
        return -1;
    }

    return static_cast<qintptr>(sock);
#else
    int sock = ::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP);
    if (sock < 0)
        return -1;

    return static_cast<qintptr>(sock);
#endif
}

//--------------------------------------------------------------------------------------------------
int sendUdp(qintptr socket, const char* buffer, qint64 size)
{
#if defined(Q_OS_WINDOWS)
    return ::send(static_cast<SOCKET>(socket), buffer, int(size), 0);
#else
    return ::send(static_cast<int>(socket), buffer, int(size), 0);
#endif
}

//--------------------------------------------------------------------------------------------------
int recvUdp(qintptr socket, char* buffer, qint64 size)
{
#if defined(Q_OS_WINDOWS)
    return ::recv(static_cast<SOCKET>(socket), buffer, int(size), 0);
#else
    return ::recv(static_cast<int>(socket), buffer, int(size), 0);
#endif
}

//--------------------------------------------------------------------------------------------------
bool connectSocket(qintptr descriptor, const QHostAddress& address, quint16 port)
{
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(address.toIPv4Address());

#if defined(Q_OS_WINDOWS)
    return ::connect(static_cast<SOCKET>(descriptor),
                     reinterpret_cast<const sockaddr*>(&addr),
                     sizeof(addr)) == 0;
#else
    return ::connect(static_cast<int>(descriptor),
                     reinterpret_cast<const sockaddr*>(&addr),
                     sizeof(addr)) == 0;
#endif
}

} // namespace

//--------------------------------------------------------------------------------------------------
StunPeer::StunPeer(QObject* parent)
    : QObject(parent),
      timer_(new QTimer(this))
{
    LOG(INFO) << "Ctor";
    timer_->setSingleShot(true);
    connect(timer_, &QTimer::timeout, this, &StunPeer::onAttempt);
}

//--------------------------------------------------------------------------------------------------
StunPeer::~StunPeer()
{
    LOG(INFO) << "Dtor";
    doStop();

    if (ready_socket_ != -1)
    {
        closeNativeSocket(ready_socket_);
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

    LOG(INFO) << "Stun peer starting (" << stun_host_ << ":" << stun_port_
              << ", attempt" << number_of_attempts_ << ")";

    timer_->start(std::chrono::seconds(3));
    lookup_id_ = QHostInfo::lookupHost(stun_host_, this, SLOT(onHostResolved(QHostInfo)));
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

    delete read_notifier_;
    read_notifier_ = nullptr;

    if (socket_ != -1)
    {
        closeNativeSocket(socket_);
        socket_ = -1;
    }
}

//--------------------------------------------------------------------------------------------------
void StunPeer::onHostResolved(const QHostInfo& host_info)
{
    lookup_id_ = -1;

    if (host_info.error() != QHostInfo::NoError)
    {
        LOG(ERROR) << "DNS lookup failed:" << host_info.errorString();
        onErrorOccurred(FROM_HERE);
        return;
    }

    const QList<QHostAddress>& addresses = host_info.addresses();
    if (addresses.isEmpty())
    {
        LOG(ERROR) << "DNS lookup returned no addresses";
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
        LOG(ERROR) << "No IPv4 address found for" << stun_host_;
        onErrorOccurred(FROM_HERE);
        return;
    }

    LOG(INFO) << "Resolved" << stun_host_ << "to" << selected;

    // Create raw UDP socket.
    socket_ = createUdpSocket();
    if (socket_ == -1)
    {
        LOG(ERROR) << "Failed to create UDP socket";
        onErrorOccurred(FROM_HERE);
        return;
    }

    // Connect to the STUN server.
    if (!connectSocket(socket_, selected, stun_port_))
    {
        LOG(ERROR) << "Failed to connect to" << selected << ":" << stun_port_;
        onErrorOccurred(FROM_HERE);
        return;
    }

    LOG(INFO) << "Connected to" << selected << ":" << stun_port_;

    // Send the STUN request.
    transaction_id_ = Random::number32();

    proto::stun::PeerToStun message;
    proto::stun::EndpointRequest* request = message.mutable_endpoint_request();
    request->set_magic_number(0xA0B1C2D3);
    request->set_transaction_id(transaction_id_);

    QByteArray buffer = base::serialize(message);
    if (buffer.isEmpty())
    {
        LOG(ERROR) << "Failed to serialize STUN request";
        onErrorOccurred(FROM_HERE);
        return;
    }

    int sent = sendUdp(socket_, buffer.constData(), buffer.size());
    if (sent != buffer.size())
    {
        LOG(ERROR) << "Failed to send STUN request";
        onErrorOccurred(FROM_HERE);
        return;
    }

    // Set up read notification.
    read_notifier_ = new QSocketNotifier(socket_, QSocketNotifier::Read, this);
    connect(read_notifier_, &QSocketNotifier::activated, this, &StunPeer::onReadyRead);
}

//--------------------------------------------------------------------------------------------------
void StunPeer::onReadyRead()
{
    static const int kMaxRecvSize = 1024;
    char recv_buffer[kMaxRecvSize];

    int received = recvUdp(socket_, recv_buffer, kMaxRecvSize);
    if (received <= 0)
    {
        LOG(ERROR) << "Failed to receive STUN response";
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

    LOG(INFO) << "External endpoint:" << ip_address << ":" << port;

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

    // Clean up the notifier (it references the socket we just gave away).
    delete read_notifier_;
    read_notifier_ = nullptr;

    timer_->stop();
    emit sig_channelReady(ip_address, static_cast<quint16>(port));
}

//--------------------------------------------------------------------------------------------------
void StunPeer::onErrorOccurred(const Location& location)
{
    LOG(ERROR) << "STUN error occurred" << location;
    timer_->stop();
    emit sig_errorOccurred();
}

} // namespace base
