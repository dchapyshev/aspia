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
#include "base/crypto/large_number_increment.h"
#include "base/crypto/message_decryptor.h"
#include "base/crypto/message_encryptor.h"

namespace base {

namespace {

const quint32 kKcpConv = 1;
const int kKcpUpdateIntervalMs = 10;
const int kKcpMtu = 1200;
const std::chrono::seconds kKeepAliveInterval { 30 };
const std::chrono::seconds kKeepAliveTimeout { 30 };
const quint32 kMaxMessageSize = 7 * 1024 * 1024; // 7 MB
const int kWriteQueueReservedSize = 128;
const int kUdpSendQueueReservedSize = 256;

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

//--------------------------------------------------------------------------------------------------
void resizeBuffer(QByteArray* buffer, qint64 size)
{
    if (buffer->capacity() < size)
    {
        buffer->clear();
        buffer->reserve(size);
    }

    buffer->resize(size);
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
    init();
}

//--------------------------------------------------------------------------------------------------
UdpChannel::UdpChannel(asio::ip::udp::socket&& socket, QObject* parent)
    : QObject(parent),
      resolver_(AsioEventDispatcher::ioContext()),
      socket_(std::move(socket))
{
    init();
}

//--------------------------------------------------------------------------------------------------
UdpChannel::~UdpChannel()
{
    keep_alive_timer_->stop();
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
        [this](const std::error_code& error_code, const asio::ip::udp::resolver::results_type& endpoints)
    {
        if (error_code)
        {
            if (error_code != asio::error::operation_aborted)
                onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        LOG(INFO) << "Resolved endpoints:" << endpointsToString(endpoints);

        asio::async_connect(socket_, endpoints,
            [this](const std::error_code& error_code, const asio::ip::udp::endpoint& endpoint)
        {
            if (error_code)
            {
                if (error_code != asio::error::operation_aborted)
                    onErrorOccurred(FROM_HERE, error_code);
                return;
            }

            LOG(INFO) << "Connected to" << endpointToString(endpoint);
            emit sig_connected();
        });
    });
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::send(quint8 channel_id, const QByteArray& buffer)
{
    addWriteTask(USER_DATA, channel_id, buffer);
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::setPaused(bool enable)
{
    if (paused_ == enable)
        return;

    paused_ = enable;
    if (paused_)
    {
        keep_alive_timer_->stop();
        update_timer_->stop();
        return;
    }

    update_timer_->start(kKcpUpdateIntervalMs);

    keep_alive_timer_type_ = KEEP_ALIVE_INTERVAL;
    keep_alive_timer_->start(kKeepAliveInterval);

    // Process any data that was accumulated while paused.
    processKcpRecv();

    // Resume reading if no read operation is in-flight.
    if (!reading_)
        doRead();
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::addWriteTask(quint8 type, quint8 param, const QByteArray& data)
{
    write_queue_.emplace_back(type, param, data);

    if (write_queue_.size() == 1)
        doWrite();
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::doWrite()
{
    if (!kcp_ || write_queue_.isEmpty())
        return;

    while (!write_queue_.isEmpty())
    {
        const WriteTask& task = write_queue_.front();
        const QByteArray& source_buffer = task.data();

        qint64 target_data_size =
            encryptor_ ? encryptor_->encryptedDataSize(source_buffer.size()) : source_buffer.size();

        if (target_data_size > kMaxMessageSize)
        {
            LOG(ERROR) << "Too big outgoing message:" << target_data_size;
            onErrorOccurred(FROM_HERE, std::error_code());
            return;
        }

        resizeBuffer(&write_buffer_, sizeof(Header) + target_data_size);

        Header* header = reinterpret_cast<Header*>(write_buffer_.data());
        header->type = task.type();
        header->param1 = task.param();
        header->param2 = 0;
        header->param3 = 0;
        header->length = static_cast<quint32>(target_data_size);

        if (encryptor_)
        {
            if (!encryptor_->encrypt(source_buffer.data(), source_buffer.size(),
                                      header, sizeof(Header),
                                      write_buffer_.data() + sizeof(Header)))
            {
                LOG(ERROR) << "Failed to encrypt outgoing message";
                onErrorOccurred(FROM_HERE, std::error_code());
                return;
            }
        }
        else
        {
            memcpy(write_buffer_.data() + sizeof(Header),
                   source_buffer.constData(), source_buffer.size());
        }

        int ret = ikcp_send(kcp_, write_buffer_.constData(), write_buffer_.size());
        if (ret < 0)
        {
            LOG(ERROR) << "ikcp_send failed with code:" << ret;
            onErrorOccurred(FROM_HERE, std::error_code());
            return;
        }

        write_queue_.pop_front();
    }

    // Flush KCP to send all queued segments immediately.
    ikcp_flush(kcp_);
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::setEncryptor(std::unique_ptr<MessageEncryptor> encryptor)
{
    encryptor_ = std::move(encryptor);
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::setDecryptor(std::unique_ptr<MessageDecryptor> decryptor)
{
    decryptor_ = std::move(decryptor);
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::init()
{
    write_queue_.reserve(kWriteQueueReservedSize);
    udp_send_queue_.reserve(kUdpSendQueueReservedSize);

    kcp_ = ikcp_create(kKcpConv, this);
    if (!kcp_)
    {
        LOG(ERROR) << "Failed to create KCP object";
        return;
    }

    kcp_->output = kcpOutputCallback;
    kcp_->stream = 1;

    // Set KCP to fast mode: nodelay=1, interval=10ms, fast resend=2, no congestion control.
    ikcp_nodelay(kcp_, 1, kKcpUpdateIntervalMs, 2, 1);
    ikcp_setmtu(kcp_, kKcpMtu);
    ikcp_wndsize(kcp_, 128, 128);

    update_timer_ = new QTimer(this);
    update_timer_->setTimerType(Qt::PreciseTimer);
    connect(update_timer_, &QTimer::timeout, this, &UdpChannel::doKcpUpdate);

    keep_alive_timer_ = new QTimer(this);
    keep_alive_timer_->setSingleShot(true);
    connect(keep_alive_timer_, &QTimer::timeout, this, &UdpChannel::onKeepAliveTimer);

    keep_alive_counter_.resize(sizeof(quint32));
    memset(keep_alive_counter_.data(), 0, keep_alive_counter_.size());
}

//--------------------------------------------------------------------------------------------------
// static
int UdpChannel::kcpOutputCallback(const char* buf, int len, IKCPCB* /* kcp */, void* user)
{
    UdpChannel* self = static_cast<UdpChannel*>(user);
    if (!self || !self->socket_.is_open())
        return -1;

    self->udp_send_queue_.emplace_back(buf, len);

    if (!self->udp_sending_)
        self->doUdpSend();

    return 0;
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::doUdpSend()
{
    if (udp_send_queue_.isEmpty())
    {
        udp_sending_ = false;
        return;
    }

    udp_sending_ = true;

    QByteArray data = std::move(udp_send_queue_.front());
    udp_send_queue_.pop_front();

    socket_.async_send(asio::buffer(data.constData(), data.size()),
        [this, data](const std::error_code& error_code, size_t /* bytes_transferred */)
    {
        if (error_code)
        {
            if (error_code != asio::error::operation_aborted)
                onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        doUdpSend();
    });
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
    reading_ = true;

    socket_.async_receive(asio::buffer(recv_buffer_.data(), recv_buffer_.size()),
        [this](const std::error_code& error_code, size_t bytes_transferred)
    {
        reading_ = false;

        if (error_code)
        {
            if (error_code != asio::error::operation_aborted)
                onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        if (!kcp_)
            return;

        int ret = ikcp_input(kcp_, recv_buffer_.data(), static_cast<long>(bytes_transferred));
        if (ret < 0)
        {
            LOG(ERROR) << "ikcp_input failed with code:" << ret;
            onErrorOccurred(FROM_HERE, std::error_code());
            return;
        }

        if (paused_)
            return;

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

    // Read all available data from KCP and append to the read buffer.
    while (true)
    {
        int peek_size = ikcp_peeksize(kcp_);
        if (peek_size <= 0)
            break;

        int offset = read_buffer_.size();
        read_buffer_.resize(offset + peek_size);

        int recv_size = ikcp_recv(kcp_, read_buffer_.data() + offset, peek_size);
        if (recv_size <= 0)
        {
            read_buffer_.resize(offset);
            break;
        }
    }

    // Parse complete messages from the accumulated read buffer.
    int consumed = 0;

    while (true)
    {
        int available = read_buffer_.size() - consumed;
        int total_size = static_cast<int>(sizeof(Header));

        if (available < total_size)
            break;

        Header header;
        memcpy(&header, read_buffer_.constData() + consumed, sizeof(Header));

        if (header.length > kMaxMessageSize)
        {
            LOG(ERROR) << "Too big incoming message:" << header.length;
            onErrorOccurred(FROM_HERE, std::error_code());
            return;
        }

        total_size += static_cast<int>(header.length);

        if (available < total_size)
            break;

        onMessageReceived(consumed);

        consumed += total_size;

        if (paused_)
            break;
    }

    // Remove consumed data from the read buffer.
    if (consumed > 0)
    {
        read_buffer_.remove(0, consumed);
    }
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::onMessageReceived(int offset)
{
    Header header;
    memcpy(&header, read_buffer_.constData() + offset, sizeof(Header));

    const char* data = read_buffer_.constData() + offset + sizeof(Header);
    int size = static_cast<int>(header.length);

    if (decryptor_)
    {
        resizeBuffer(&decrypt_buffer_, decryptor_->decryptedDataSize(size));

        if (!decryptor_->decrypt(data, size,
                                 &header, sizeof(Header),
                                 decrypt_buffer_.data()))
        {
            LOG(ERROR) << "Failed to decrypt incoming message";
            onErrorOccurred(FROM_HERE, std::error_code());
            return;
        }
    }

    const char* payload_data = decryptor_ ? decrypt_buffer_.constData() : data;
    int payload_size = decryptor_ ? decrypt_buffer_.size() : size;

    if (header.type == USER_DATA)
    {
        emit sig_messageReceived(header.param1,
                                 QByteArray(payload_data, payload_size));
    }
    else if (header.type == KEEP_ALIVE)
    {
        QByteArray payload(payload_data, payload_size);

        if (header.param1 & KEEP_ALIVE_PING)
        {
            addWriteTask(KEEP_ALIVE, KEEP_ALIVE_PONG, payload);
            return;
        }

        if (payload.size() != keep_alive_counter_.size())
        {
            LOG(ERROR) << "Invalid keep-alive payload size";
            onErrorOccurred(FROM_HERE, std::error_code());
            return;
        }

        // Pong must contain the same data as ping.
        if (payload != keep_alive_counter_)
        {
            LOG(ERROR) << "Keep-alive counter mismatch";
            onErrorOccurred(FROM_HERE, std::error_code());
            return;
        }

        // Increase the counter of sent packets.
        largeNumberIncrement(&keep_alive_counter_);

        // Restart keep alive timer.
        keep_alive_timer_type_ = KEEP_ALIVE_INTERVAL;
        keep_alive_timer_->start(kKeepAliveInterval);
    }
    else
    {
        // Ignore unknown types.
    }
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::onKeepAliveTimer()
{
    if (keep_alive_timer_type_ == KEEP_ALIVE_INTERVAL)
    {
        addWriteTask(KEEP_ALIVE, KEEP_ALIVE_PING, keep_alive_counter_);
        keep_alive_timer_type_ = KEEP_ALIVE_TIMEOUT;
        keep_alive_timer_->start(kKeepAliveTimeout);
    }
    else
    {
        DCHECK_EQ(keep_alive_timer_type_, KEEP_ALIVE_TIMEOUT);
        LOG(ERROR) << "Keep-alive timeout, closing connection";
        onErrorOccurred(FROM_HERE, std::error_code());
    }
}

} // namespace base
