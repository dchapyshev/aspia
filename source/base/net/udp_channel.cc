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

#include "base/net/udp_channel.h"

#include <QTimer>

#include <asio/connect.hpp>

#include <ikcp.h>

#include "base/asio_event_dispatcher.h"
#include "base/location.h"
#include "base/logging.h"

namespace base {

namespace {

const quint32 kKcpConv = 1;
const int kKcpUpdateIntervalMs = 10;
const int kKcpMtu = 1400;

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
QStringList endpointsToString(const asio::ip::udp::resolver::results_type& endpoints)
{
    if (endpoints.empty())
        return QStringList();

    QStringList list;
    list.resize(endpoints.size());

    for (auto it = endpoints.begin(), it_end = endpoints.end(); it != it_end; ++it)
        list.emplace_back(endpointToString(it->endpoint()));

    return list;
}

//--------------------------------------------------------------------------------------------------
quint32 currentTimeMs()
{
    return static_cast<quint32>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count() & 0xFFFFFFFF);
}

} // namespace

//--------------------------------------------------------------------------------------------------
UdpChannel::UdpChannel(QObject* parent)
    : QObject(parent),
      resolver_(AsioEventDispatcher::ioContext()),
      socket_(AsioEventDispatcher::ioContext())
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
UdpChannel::UdpChannel(asio::ip::udp::socket&& socket, QObject* parent)
    : QObject(parent),
      resolver_(AsioEventDispatcher::ioContext()),
      socket_(std::move(socket))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
UdpChannel::~UdpChannel()
{
    if (update_timer_)
        update_timer_->stop();

    resolver_.cancel();

    std::error_code ignored_code;
    socket_.cancel(ignored_code);
    socket_.close(ignored_code);

    if (kcp_)
    {
        ikcp_release(kcp_);
        kcp_ = nullptr;
    }
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::connectTo(const QString& address, quint16 port)
{
    resolver_.async_resolve(address.toLocal8Bit().toStdString(), std::to_string(port),
        [&](const std::error_code& error_code, const asio::ip::udp::resolver::results_type& endpoints)
    {
        if (error_code)
        {
            if (error_code != asio::error::operation_aborted)
                onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        LOG(INFO) << "Resolved endpoints:" << endpointsToString(endpoints);

        asio::async_connect(socket_, endpoints,
            [&](const std::error_code& error_code, const asio::ip::udp::endpoint& endpoint)
        {
            if (error_code)
            {
                if (error_code != asio::error::operation_aborted)
                    onErrorOccurred(FROM_HERE, error_code);
                return;
            }

            LOG(INFO) << "Connected to" << endpointToString(endpoint);
            onConnected();
        });
    });
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::send(quint8 channel_id, const QByteArray& buffer)
{
    if (!kcp_)
        return;

    // Prepend channel_id to the data.
    QByteArray message;
    message.reserve(1 + buffer.size());
    message.append(static_cast<char>(channel_id));
    message.append(buffer);

    int ret = ikcp_send(kcp_, message.constData(), message.size());
    if (ret < 0)
    {
        LOG(ERROR) << "ikcp_send failed with code:" << ret;
        onErrorOccurred(FROM_HERE, std::error_code());
        return;
    }

    // Flush immediately to reduce latency.
    ikcp_flush(kcp_);
}

//--------------------------------------------------------------------------------------------------
// static
int UdpChannel::kcpOutputCallback(const char* buf, int len, IKCPCB* /* kcp */, void* user)
{
    UdpChannel* self = static_cast<UdpChannel*>(user);
    if (!self || !self->socket_.is_open())
        return -1;

    // Copy the data since async_send may complete after this callback returns.
    auto data = std::make_shared<std::vector<uint8_t>>(buf, buf + len);

    self->socket_.async_send(asio::buffer(data->data(), data->size()),
        [self, data](const std::error_code& error_code, size_t /* bytes_transferred */)
    {
        if (error_code)
        {
            if (error_code != asio::error::operation_aborted)
                self->onErrorOccurred(FROM_HERE, error_code);
        }
    });

    return 0;
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::initKcp()
{
    kcp_ = ikcp_create(kKcpConv, this);
    if (!kcp_)
    {
        LOG(ERROR) << "Failed to create KCP object";
        onErrorOccurred(FROM_HERE, std::error_code());
        return;
    }

    kcp_->output = kcpOutputCallback;

    // Set KCP to fast mode: nodelay=1, interval=10ms, fast resend=2, no congestion control.
    ikcp_nodelay(kcp_, 1, kKcpUpdateIntervalMs, 2, 1);
    ikcp_setmtu(kcp_, kKcpMtu);
    ikcp_wndsize(kcp_, 128, 128);

    // Start the update timer.
    update_timer_ = new QTimer(this);
    update_timer_->setTimerType(Qt::PreciseTimer);
    connect(update_timer_, &QTimer::timeout, this, &UdpChannel::doKcpUpdate);
    update_timer_->start(kKcpUpdateIntervalMs);
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::onConnected()
{
    initKcp();
    doRead();
    emit sig_connected();
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::onErrorOccurred(const Location& location, const std::error_code& error_code)
{
    if (error_code)
        LOG(ERROR) << "Error occurred" << error_code << "from" << location.toString();
    else
        LOG(ERROR) << "Error occurred from" << location.toString();

    emit sig_errorOccurred();
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::doRead()
{
    socket_.async_receive(asio::buffer(recv_buffer_.data(), recv_buffer_.size()),
        [this](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (error_code)
        {
            if (error_code != asio::error::operation_aborted)
                onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        if (!kcp_)
            return;

        int ret = ikcp_input(kcp_, reinterpret_cast<const char*>(recv_buffer_.data()),
                             static_cast<long>(bytes_transferred));
        if (ret < 0)
        {
            LOG(ERROR) << "ikcp_input failed with code:" << ret;
            onErrorOccurred(FROM_HERE, std::error_code());
            return;
        }

        processKcpRecv();

        // Continue reading.
        doRead();
    });
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::doKcpUpdate()
{
    if (!kcp_)
        return;

    ikcp_update(kcp_, currentTimeMs());
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::processKcpRecv()
{
    if (!kcp_)
        return;

    while (true)
    {
        int peek_size = ikcp_peeksize(kcp_);
        if (peek_size < 0)
            break;

        QByteArray buffer;
        buffer.resize(peek_size);

        int recv_size = ikcp_recv(kcp_, buffer.data(), buffer.size());
        if (recv_size < 0)
            break;

        // The first byte is the channel_id.
        if (recv_size < 1)
        {
            LOG(ERROR) << "Received empty KCP message";
            continue;
        }

        quint8 channel_id = static_cast<quint8>(buffer.at(0));
        QByteArray payload = buffer.mid(1);

        emit sig_messageReceived(channel_id, payload);
    }
}

} // namespace base
