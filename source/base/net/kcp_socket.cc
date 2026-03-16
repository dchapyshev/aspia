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

#include "base/net/kcp_socket.h"

#include <QTimerEvent>

#include <asio/connect.hpp>
#include <ikcp.h>

#include "base/asio_event_dispatcher.h"
#include "base/logging.h"

namespace base {

namespace {

const quint32 kKcpConv = 1;
const int kKcpInitialUpdateMs = 10;
const int kKcpMtu = 1200;
const int kMaxKcpChunk = 120 * (kKcpMtu - 24);
const int kKcpWindowSize = 512;
const int kKcpReadBufferReserve = 512 * 1024;
const int kUdpSendQueueReservedSize = 256;

//--------------------------------------------------------------------------------------------------
quint32 currentTimeMs()
{
    return static_cast<quint32>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count() & 0xFFFFFFFF);
}

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
    list.reserve(endpoints.size());

    for (auto it = endpoints.begin(), it_end = endpoints.end(); it != it_end; ++it)
        list.emplace_back(endpointToString(it->endpoint()));

    return list;
}

} // namespace

//--------------------------------------------------------------------------------------------------
KcpSocket::KcpSocket(QObject* parent)
    : QObject(parent),
      resolver_(AsioEventDispatcher::ioContext()),
      socket_(AsioEventDispatcher::ioContext())
{
    init();
}

//--------------------------------------------------------------------------------------------------
KcpSocket::KcpSocket(asio::ip::udp::socket&& socket, QObject* parent)
    : QObject(parent),
      resolver_(AsioEventDispatcher::ioContext()),
      socket_(std::move(socket))
{
    init();
}

//--------------------------------------------------------------------------------------------------
KcpSocket::~KcpSocket()
{
    // Mark guard before releasing resources so that any pending ASIO handlers
    // (already completed but not yet dispatched) will see the object is gone.
    *alive_guard_ = false;

    close();
    ikcp_release(kcp_);
}

//--------------------------------------------------------------------------------------------------
// static
KcpSocket* KcpSocket::bind(quint16& port)
{
    asio::ip::udp::endpoint endpoint(asio::ip::address_v6::any(), port);

    std::error_code error_code;
    asio::ip::udp::socket socket(AsioEventDispatcher::ioContext());
    socket.open(endpoint.protocol(), error_code);
    if (error_code)
    {
        LOG(ERROR) << "Unable to open UDP socket on port" << port << ':' << error_code;
        return nullptr;
    }

    socket.bind(endpoint, error_code);
    if (error_code)
    {
        LOG(ERROR) << "Unable to bind UDP port" << port << ':' << error_code;
        return nullptr;
    }

    if (!port)
    {
        std::error_code error_code;
        asio::ip::udp::endpoint local_endpoint = socket.local_endpoint(error_code);
        if (error_code)
        {
            LOG(ERROR) << "Unable to get binded endpoint:" << error_code;
            return nullptr;
        }

        port = local_endpoint.port();
    }

    return new KcpSocket(std::move(socket));
}

//--------------------------------------------------------------------------------------------------
void KcpSocket::init()
{
    udp_send_queue_.reserve(kUdpSendQueueReservedSize);
    kcp_read_buffer_.reserve(kKcpReadBufferReserve);

    kcp_ = ikcp_create(kKcpConv, this);
    CHECK(kcp_);

    kcp_->output = kcpOutputCallback;
    kcp_->writelog = kcpWriteLogCallback;
    kcp_->stream = 1;

    // Set KCP to fast mode: nodelay=1, interval=10ms, fast resend=2, no congestion control.
    ikcp_nodelay(kcp_, 1, kKcpInitialUpdateMs, 2, 1);
    ikcp_setmtu(kcp_, kKcpMtu);
    ikcp_wndsize(kcp_, kKcpWindowSize, kKcpWindowSize);

    // Start the KCP update timer. Reading will begin automatically once the socket is open.
    update_timer_id_ = startTimer(kKcpInitialUpdateMs, Qt::PreciseTimer);
}

//--------------------------------------------------------------------------------------------------
void KcpSocket::connectTo(const QString& address, quint16 port)
{
    auto guard = alive_guard_;
    resolver_.async_resolve(address.toLocal8Bit().toStdString(), std::to_string(port),
        [this, guard](const std::error_code& error_code, const asio::ip::udp::resolver::results_type& endpoints)
    {
        if (!*guard)
            return;

        if (error_code)
        {
            if (error_code != asio::error::operation_aborted)
            {
                LOG(ERROR) << "DNS resolve error:" << error_code;
                emit sig_errorOccurred();
            }
            return;
        }

        LOG(INFO) << "Resolved endpoints:" << endpointsToString(endpoints);

        asio::async_connect(socket_, endpoints,
            [this, guard](const std::error_code& error_code, const asio::ip::udp::endpoint& endpoint)
        {
            if (!*guard)
                return;

            if (error_code)
            {
                if (error_code != asio::error::operation_aborted)
                {
                    LOG(ERROR) << "Connect error:" << error_code;
                    emit sig_errorOccurred();
                }
                return;
            }

            LOG(INFO) << "Connected to" << endpointToString(endpoint);
            connected_ = true;
            emit sig_connected();
        });
    });
}

//--------------------------------------------------------------------------------------------------
bool KcpSocket::send(const char* data, int size)
{
    const char* ptr = data;
    int remaining = size;

    while (remaining > 0)
    {
        int chunk = std::min(remaining, kMaxKcpChunk);
        int ret = ikcp_send(kcp_, ptr, chunk);
        if (ret < 0)
        {
            LOG(ERROR) << "ikcp_send failed with code:" << ret;
            emit sig_errorOccurred();
            return false;
        }
        ptr += chunk;
        remaining -= chunk;
    }

    // Flush KCP to send all queued segments immediately.
    ikcp_flush(kcp_);
    return true;
}

//--------------------------------------------------------------------------------------------------
void KcpSocket::close()
{
    if (update_timer_id_ != 0)
    {
        killTimer(update_timer_id_);
        update_timer_id_ = 0;
    }

    resolver_.cancel();

    std::error_code ignored_code;
    socket_.cancel(ignored_code);
    socket_.close(ignored_code);
}

//--------------------------------------------------------------------------------------------------
quint16 KcpSocket::port() const
{
    std::error_code error_code;
    asio::ip::udp::endpoint endpoint = socket_.local_endpoint(error_code);
    if (error_code)
    {
        LOG(ERROR) << "Unable to get local endpoint:" << error_code;
        return 0;
    }

    return endpoint.port();
}

//--------------------------------------------------------------------------------------------------
void KcpSocket::doRead()
{
    reading_ = true;

    if (connected_)
    {
        auto guard = alive_guard_;
        socket_.async_receive(asio::buffer(recv_buffer_.data(), recv_buffer_.size()),
            [this, guard](const std::error_code& error_code, size_t bytes_transferred)
        {
            if (!*guard)
                return;

            if (error_code)
            {
                if (error_code != asio::error::operation_aborted)
                {
                    LOG(ERROR) << "UDP receive error:" << error_code;
                    emit sig_errorOccurred();
                }
                return;
            }

            reading_ = false;
            onUdpDataReceived(bytes_transferred);
        });
        return;
    }

    auto guard = alive_guard_;
    socket_.async_receive_from(asio::buffer(recv_buffer_.data(), recv_buffer_.size()), remote_endpoint_,
        [this, guard](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (!*guard)
            return;

        if (error_code)
        {
            if (error_code != asio::error::operation_aborted)
            {
                LOG(ERROR) << "UDP receive error:" << error_code;
                emit sig_errorOccurred();
            }
            return;
        }

        reading_ = false;

        // Connect to the actual peer now that we know its address.
        std::error_code connect_ec;
        socket_.connect(remote_endpoint_, connect_ec);
        if (connect_ec)
        {
            LOG(ERROR) << "Failed to connect to peer:" << connect_ec;
            emit sig_errorOccurred();
            return;
        }

        LOG(INFO) << "Peer connected:" << endpointToString(remote_endpoint_);
        connected_ = true;

        // Flush any UDP packets that were buffered while waiting for the peer.
        if (!udp_sending_ && !udp_send_queue_.isEmpty())
            doUdpSend();

        emit sig_connected();
        onUdpDataReceived(bytes_transferred);
    });
}

//--------------------------------------------------------------------------------------------------
void KcpSocket::onUdpDataReceived(size_t bytes_transferred)
{
    int ret = ikcp_input(kcp_, recv_buffer_.data(), static_cast<int>(bytes_transferred));
    if (ret < 0)
    {
        LOG(ERROR) << "ikcp_input failed with code:" << ret;
        emit sig_errorOccurred();
        return;
    }

    int total_read = 0;

    while (true)
    {
        int peek_size = ikcp_peeksize(kcp_);
        if (peek_size <= 0)
            break;

        int required = total_read + peek_size;
        if (kcp_read_buffer_.size() < required)
            kcp_read_buffer_.resize(required);

        int recv_size = ikcp_recv(kcp_, kcp_read_buffer_.data() + total_read, peek_size);
        if (recv_size <= 0)
            break;

        total_read += recv_size;
    }

    if (total_read > 0)
        emit sig_dataReceived(kcp_read_buffer_.constData(), total_read);

    if (socket_.is_open())
        doRead();
}

//--------------------------------------------------------------------------------------------------
// static
int KcpSocket::kcpOutputCallback(const char* buf, int len, IKCPCB* /* kcp */, void* user)
{
    KcpSocket* self = static_cast<KcpSocket*>(user);
    if (!self || !self->socket_.is_open())
        return -1;

    self->udp_send_queue_.emplace_back(buf, len);

    if (!self->udp_sending_)
        self->doUdpSend();

    return 0;
}

//--------------------------------------------------------------------------------------------------
// static
void KcpSocket::kcpWriteLogCallback(const char* log, IKCPCB* /* kcp */, void* /* user */)
{
    LOG(INFO) << log;
}

//--------------------------------------------------------------------------------------------------
void KcpSocket::doUdpSend()
{
    if (udp_send_queue_.isEmpty())
    {
        udp_sending_ = false;
        return;
    }

    if (!connected_)
    {
        // Can't send until we know the peer's address (learned on first receive).
        // Keep packets in the queue; they will be sent once the peer connects.
        udp_sending_ = false;
        return;
    }

    udp_sending_ = true;

    // Move into member to keep data alive during async_send without lambda capture copy.
    udp_send_active_ = std::move(udp_send_queue_.front());
    udp_send_queue_.pop_front();

    auto guard = alive_guard_;
    socket_.async_send(asio::buffer(udp_send_active_.constData(), udp_send_active_.size()),
        [this, guard](const std::error_code& error_code, size_t /* bytes_transferred */)
    {
        if (!*guard)
            return;

        if (error_code)
        {
            if (error_code != asio::error::operation_aborted)
            {
                LOG(ERROR) << "UDP send error:" << error_code;
                emit sig_errorOccurred();
            }
            return;
        }

        doUdpSend();
    });
}

//--------------------------------------------------------------------------------------------------
void KcpSocket::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == update_timer_id_)
        doKcpUpdate();
}

//--------------------------------------------------------------------------------------------------
void KcpSocket::doKcpUpdate()
{
    if (!socket_.is_open())
        return;

    ikcp_update(kcp_, currentTimeMs());

    // Auto-start reading once the socket is open.
    if (!reading_)
    {
        // Detect if the socket was connected externally (client path via async_connect).
        if (!connected_)
        {
            std::error_code ec;
            socket_.remote_endpoint(ec);
            if (!ec)
                connected_ = true;
        }

        doRead();
    }
}

} // namespace base
