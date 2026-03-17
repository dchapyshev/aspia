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
const int kKcpUpdateIntervalMs = 10;
const int kKcpMtu = 1200;
const int kMaxKcpChunk = 120 * (kKcpMtu - 24);
const int kKcpWindowSize = 512;
const int kKcpReadBufferReserve = 512 * 1024;
const int kUdpSendQueueReservedSize = 256;
const int kPoolReservedSize = 256;
const int kPoolBufferReserved = 2 * 1024;
const quint32 kMagicNumber = 0xDC8894AC;

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

} // namespace

//--------------------------------------------------------------------------------------------------
KcpSocket::KcpSocket(QObject* parent)
    : QObject(parent),
      socket_(AsioEventDispatcher::ioContext())
{
    init();
}

//--------------------------------------------------------------------------------------------------
KcpSocket::KcpSocket(asio::ip::udp::socket&& socket, QObject* parent)
    : QObject(parent),
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

    return new KcpSocket(std::move(socket), nullptr);
}

//--------------------------------------------------------------------------------------------------
void KcpSocket::connectTo(const QString& address, quint16 port)
{
    std::error_code error_code;
    asio::ip::address ip_address =
        asio::ip::make_address(address.toLocal8Bit().toStdString(), error_code);
    if (error_code)
    {
        LOG(ERROR) << "Unable to parse address:" << error_code;
        emit sig_errorOccurred();
        return;
    }

    asio::ip::udp::endpoint remote_endpoint(ip_address, port);

    socket_.open(remote_endpoint.protocol(), error_code);
    if (error_code)
    {
        LOG(ERROR) << "Unable to open socket:" << error_code;
        emit sig_errorOccurred();
        return;
    }

    socket_.connect(remote_endpoint, error_code);
    if (error_code)
    {
        LOG(ERROR) << "Unable to connect socket:" << error_code;
        emit sig_errorOccurred();
        return;
    }

    has_remote_endpoint_ = true;
    sendHandshake();
}

//--------------------------------------------------------------------------------------------------
bool KcpSocket::send(const void* data, int size)
{
    const char* ptr = reinterpret_cast<const char*>(data);
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

    std::error_code ignored_code;
    socket_.cancel(ignored_code);
    socket_.close(ignored_code);
}

//--------------------------------------------------------------------------------------------------
void KcpSocket::timerEvent(QTimerEvent* event)
{
    if (event->timerId() != update_timer_id_)
        return;

    quint32 current_time = currentTimeMs();

    ikcp_update(kcp_, current_time);

    quint32 next_time = ikcp_check(kcp_, current_time);
    if (next_time - current_time < kKcpUpdateIntervalMs)
        ikcp_flush(kcp_);

    // Auto-start reading once the socket is open.
    if (!udp_reading_)
        doRead();
}

//--------------------------------------------------------------------------------------------------
// static
int KcpSocket::kcpOutputCallback(const char* buf, int len, IKCPCB* /* kcp */, void* user)
{
    KcpSocket* self = static_cast<KcpSocket*>(user);
    DCHECK(self);

    QByteArray buffer = self->acquireBuffer();
    buffer.resize(len);
    memcpy(buffer.data(), buf, len);

    self->udp_send_queue_.emplace_back(std::move(buffer));
    self->doUdpSend();
    return 0;
}

//--------------------------------------------------------------------------------------------------
void KcpSocket::init()
{
    udp_send_queue_.reserve(kUdpSendQueueReservedSize);
    kcp_read_buffer_.reserve(kKcpReadBufferReserve);
    udp_buffer_pool_.reserve(kPoolReservedSize);

    kcp_ = ikcp_create(kKcpConv, this);
    CHECK(kcp_);

    kcp_->output = kcpOutputCallback;
    kcp_->stream = 1;

    // Set KCP to fast mode: nodelay=1, interval=10ms, fast resend=2, no congestion control.
    ikcp_nodelay(kcp_, 1, kKcpUpdateIntervalMs, 2, 1);
    ikcp_setmtu(kcp_, kKcpMtu);
    ikcp_wndsize(kcp_, kKcpWindowSize, kKcpWindowSize);

    // Start the KCP update timer. Reading will begin automatically once the socket is open.
    update_timer_id_ = startTimer(kKcpUpdateIntervalMs, Qt::PreciseTimer);
}

//--------------------------------------------------------------------------------------------------
void KcpSocket::doRead()
{
    udp_reading_ = true;

    if (has_remote_endpoint_)
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

            udp_reading_ = false;
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

        // Connect to the actual peer now that we know its address.
        std::error_code connect_code;
        socket_.connect(remote_endpoint_, connect_code);
        if (connect_code)
        {
            LOG(ERROR) << "Failed to connect to peer:" << connect_code;
            emit sig_errorOccurred();
            return;
        }

        LOG(INFO) << "Far peer endpoint has been received:" << endpointToString(remote_endpoint_);
        has_remote_endpoint_ = true;

        // Flush any UDP packets that were buffered while waiting for the peer.
        doUdpSend();

        udp_reading_ = false;
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
    {
        if (has_remote_endpoint_ && !ready_)
        {
            if (total_read < static_cast<int>(sizeof(kMagicNumber)))
            {
                LOG(ERROR) << "Handshake data too short:" << total_read;
                emit sig_errorOccurred();
                return;
            }

            quint32 received_magic;
            memcpy(&received_magic, kcp_read_buffer_.constData(), sizeof(kMagicNumber));

            if (received_magic != kMagicNumber)
            {
                LOG(ERROR) << "Invalid handshake magic number";
                emit sig_errorOccurred();
                return;
            }

            // Server side: respond with our own handshake.
            if (!handshake_sent_)
                sendHandshake();

            LOG(INFO) << "Handshake completed";
            ready_ = true;
            emit sig_ready();

            // Forward any data that arrived after the magic number.
            int remaining = total_read - static_cast<int>(sizeof(kMagicNumber));
            if (remaining > 0)
            {
                emit sig_dataReceived(kcp_read_buffer_.constData() + sizeof(kMagicNumber), remaining);
            }
        }
        else
        {
            emit sig_dataReceived(kcp_read_buffer_.constData(), total_read);
        }
    }

    doRead();
}

//--------------------------------------------------------------------------------------------------
void KcpSocket::doUdpSend()
{
    // Can't send until we know the peer's address (learned on first receive).
    // Keep packets in the queue; they will be sent once the peer connects.
    if (udp_send_queue_.isEmpty() || udp_sending_ || !has_remote_endpoint_)
        return;

    udp_send_active_ = udp_send_queue_.dequeue();
    udp_sending_ = true;

    auto guard = alive_guard_;
    socket_.async_send(asio::buffer(udp_send_active_.constData(), udp_send_active_.size()),
        [this, guard](const std::error_code& error_code, size_t /* bytes_transferred */)
    {
        if (!*guard)
            return;

        releaseBuffer(std::move(udp_send_active_));

        if (error_code)
        {
            if (error_code != asio::error::operation_aborted)
            {
                LOG(ERROR) << "UDP send error:" << error_code;
                emit sig_errorOccurred();
            }
            return;
        }

        udp_sending_ = false;
        doUdpSend();
    });
}

//--------------------------------------------------------------------------------------------------
void KcpSocket::sendHandshake()
{
    handshake_sent_ = true;
    send(&kMagicNumber, sizeof(kMagicNumber));
}

//--------------------------------------------------------------------------------------------------
QByteArray KcpSocket::acquireBuffer()
{
    if (!udp_buffer_pool_.isEmpty())
        return udp_buffer_pool_.takeLast();

    QByteArray buffer;
    buffer.reserve(kPoolBufferReserved);
    return buffer;
}

//--------------------------------------------------------------------------------------------------
void KcpSocket::releaseBuffer(QByteArray&& buffer)
{
    if (udp_buffer_pool_.size() < kPoolReservedSize)
        udp_buffer_pool_.emplace_back(std::move(buffer));
}

} // namespace base
