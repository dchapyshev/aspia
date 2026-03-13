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
#include <QTimer>

#include <asio/connect.hpp>

#include "base/asio_event_dispatcher.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/net/udp_channel.h"
#include "proto/stun.h"

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
QString addressToString(const asio::ip::address& address)
{
    return QString::fromLocal8Bit(address.to_string());
}

//--------------------------------------------------------------------------------------------------
QString endpointToString(const asio::ip::udp::endpoint& endpoint)
{
    return addressToString(endpoint.address()) + ':' + QString::number(endpoint.port());
}

//--------------------------------------------------------------------------------------------------
bool isValidIpAddress(const QString& ip_address)
{
    if (ip_address.isEmpty())
        return false;

    QHostAddress address(ip_address);
    if (address.protocol() == QAbstractSocket::IPv4Protocol ||
        address.protocol() == QAbstractSocket::IPv6Protocol)
    {
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
bool isValidPort(quint32 port)
{
    return port != 0 && port <= 65535;
}

} // namespace

//--------------------------------------------------------------------------------------------------
StunPeer::StunPeer(QObject* parent)
    : QObject(parent),
      timer_(new QTimer(this)),
      udp_resolver_(AsioEventDispatcher::ioContext()),
      udp_socket_(AsioEventDispatcher::ioContext())
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
}

//--------------------------------------------------------------------------------------------------
void StunPeer::start(const QString& stun_host, quint16 stun_port)
{
    stun_host_ = stun_host.toLocal8Bit().toStdString();
    stun_port_ = std::to_string(stun_port);
    doStart();
}

//--------------------------------------------------------------------------------------------------
UdpChannel* StunPeer::takeChannel()
{
    UdpChannel* channel = ready_channel_;
    channel->setParent(nullptr);
    ready_channel_ = nullptr;
    return channel;
}

//--------------------------------------------------------------------------------------------------
void StunPeer::onAttempt()
{
    static const int kMaxAttemptsCount = 3;

    if (number_of_attempts_ > kMaxAttemptsCount)
    {
        onErrorOccurred(FROM_HERE, std::error_code());
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

    udp_resolver_.async_resolve(stun_host_, stun_port_,
        [&](const std::error_code& error_code,
            const asio::ip::udp::resolver::results_type& endpoints)
    {
        if (error_code)
        {
            if (error_code != asio::error::operation_aborted)
                onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        LOG(INFO) << "Resolved endpoints:";
        for (const auto& it : endpoints)
            LOG(INFO) << endpointToString(it.endpoint());

        asio::async_connect(udp_socket_, endpoints,
            [&](const std::error_code& error_code, const asio::ip::udp::endpoint& endpoint)
        {
            if (error_code)
            {
                if (error_code != asio::error::operation_aborted)
                    onErrorOccurred(FROM_HERE, error_code);
                return;
            }

            LOG(INFO) << "Connected to" << endpointToString(endpoint);
            doReceiveExternalAddress();
        });
    });
}

//--------------------------------------------------------------------------------------------------
void StunPeer::doStop()
{
    timer_->stop();
    udp_resolver_.cancel();

    std::error_code ignored_code;
    udp_socket_.cancel(ignored_code);
    udp_socket_.close(ignored_code);
}

//--------------------------------------------------------------------------------------------------
void StunPeer::doReceiveExternalAddress()
{
    proto::stun::PeerToStun message;
    message.mutable_endpoint_request()->set_magic_number(0xA0B1C2D3);

    const size_t size = message.ByteSizeLong();
    if (!size || size > buffer_.size())
    {
        LOG(ERROR) << "Invalid message size:" << size;
        onErrorOccurred(FROM_HERE, std::error_code());
        return;
    }

    message.SerializeWithCachedSizesToArray(buffer_.data());

    udp_socket_.async_send(asio::buffer(buffer_.data(), size),
        [this](const std::error_code& error_code, size_t /* bytes_transferred */)
    {
        if (error_code)
        {
            if (error_code != asio::error::operation_aborted)
                onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        udp_socket_.async_receive(asio::buffer(buffer_.data(), buffer_.size()),
            [this](const std::error_code& error_code, size_t bytes_transferred)
        {
            if (error_code != asio::error::operation_aborted)
            {
                onErrorOccurred(FROM_HERE, error_code);
                return;
            }

            proto::stun::StunToPeer message;
            if (!message.ParseFromArray(buffer_.data(), static_cast<int>(bytes_transferred)))
            {
                onErrorOccurred(FROM_HERE, error_code);
                return;
            }

            if (!message.has_endpoint())
            {
                onErrorOccurred(FROM_HERE, error_code);
                return;
            }

            const proto::stun::Endpoint& endpoint = message.endpoint();

            QString ip_address = QString::fromStdString(endpoint.ip_address());
            quint32 port = static_cast<quint16>(endpoint.port());

            LOG(INFO) << "External endpoint:" << ip_address << ":" << port;

            if (!isValidIpAddress(ip_address))
            {
                onErrorOccurred(FROM_HERE, error_code);
                return;
            }

            if (!isValidPort(port))
            {
                onErrorOccurred(FROM_HERE, error_code);
                return;
            }

            ready_channel_ = new UdpChannel(std::move(udp_socket_), this);
            timer_->stop();

            emit sig_channelReady(ip_address, static_cast<quint16>(port));
        });
    });
}

//--------------------------------------------------------------------------------------------------
void StunPeer::onErrorOccurred(const base::Location& location, const std::error_code& error_code)
{
    if (error_code)
        LOG(ERROR) << "Error occurred" << error_code << "from" << location.toString();
    else
        LOG(ERROR) << "Error occurred from" << location.toString();

    timer_->stop();
    emit sig_errorOccurred();
}

} // namespace base
