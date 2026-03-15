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

#include "base/location.h"
#include "base/logging.h"
#include "base/crypto/large_number_increment.h"
#include "base/crypto/message_decryptor.h"
#include "base/crypto/message_encryptor.h"
#include "base/net/kcp_socket.h"

namespace base {

namespace {

const std::chrono::seconds kKeepAliveInterval { 30 };
const std::chrono::seconds kKeepAliveTimeout { 30 };
const quint32 kMaxMessageSize = 7 * 1024 * 1024; // 7 MB
const int kWriteQueueReservedSize = 128;
const int kReadBufferReserveSize = 512 * 1024;
const int kWriteBufferReserveSize = 2 * 1024 * 1024;
const int kDecryptBufferReserveSize = 2 * 1024 * 1024;
const int kReadBufferCompactThreshold = 64 * 1024;

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
int calculateSpeed(int last_speed, const std::chrono::milliseconds& duration, qint64 bytes)
{
    static const double kAlpha = 0.1;
    return static_cast<int>(
        (kAlpha * ((1000.0 / static_cast<double>(duration.count())) * static_cast<double>(bytes))) +
        ((1.0 - kAlpha) * static_cast<double>(last_speed)));
}

} // namespace

//--------------------------------------------------------------------------------------------------
UdpChannel::UdpChannel(QObject* parent)
    : QObject(parent),
      kcp_socket_(new KcpSocket(this))
{
    init();
}

//--------------------------------------------------------------------------------------------------
UdpChannel::UdpChannel(asio::ip::udp::socket&& socket, QObject* parent)
    : QObject(parent),
      kcp_socket_(new KcpSocket(std::move(socket), this))
{
    init();
}

//--------------------------------------------------------------------------------------------------
UdpChannel::~UdpChannel()
{
    keep_alive_timer_->stop();
    kcp_socket_->close();
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::connectTo(const QString& address, quint16 port)
{
    client_connect_ = true;
    kcp_socket_->connectTo(address, port);
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::send(quint8 channel_id, const QByteArray& buffer)
{
    addWriteTask(USER_DATA, channel_id, buffer);
}

//--------------------------------------------------------------------------------------------------
bool UdpChannel::isConnected() const
{
    return connected_;
}

//--------------------------------------------------------------------------------------------------
quint16 UdpChannel::port() const
{
    return kcp_socket_->port();
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
        return;
    }

    if (connected_)
    {
        // Start keep-alive only when we know the peer's address.
        keep_alive_timer_type_ = KEEP_ALIVE_INTERVAL;
        keep_alive_timer_->start(kKeepAliveInterval);
    }

    // Process any data that was accumulated while paused.
    if (!parseMessages())
        return;
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::addWriteTask(quint8 type, quint8 param, QByteArray data)
{
    write_queue_.emplace_back(type, param, std::move(data));

    if (write_queue_.size() == 1)
        doWrite();
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::doWrite()
{
    if (!kcp_socket_ || write_queue_.isEmpty())
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

        Header header;
        header.type = task.type();
        header.param1 = task.param();
        header.param2 = 0;
        header.param3 = 0;
        header.length = static_cast<quint32>(target_data_size);
        memcpy(write_buffer_.data(), &header, sizeof(Header));

        if (encryptor_)
        {
            if (!encryptor_->encrypt(source_buffer.data(), source_buffer.size(),
                                     &header, sizeof(Header),
                                     write_buffer_.data() + sizeof(Header)))
            {
                LOG(ERROR) << "Failed to encrypt outgoing message";
                onErrorOccurred(FROM_HERE, std::error_code());
                return;
            }
        }
        else
        {
            memcpy(write_buffer_.data() + sizeof(Header), source_buffer.constData(), source_buffer.size());
        }

        addTxBytes(write_buffer_.size());

        if (!kcp_socket_->send(write_buffer_.constData(), static_cast<int>(write_buffer_.size())))
        {
            onErrorOccurred(FROM_HERE, std::error_code());
            return;
        }

        write_queue_.pop_front();
    }
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
int UdpChannel::speedRx()
{
    TimePoint current_time = Clock::now();
    Milliseconds duration = std::chrono::duration_cast<Milliseconds>(current_time - begin_time_rx_);

    speed_rx_ = calculateSpeed(speed_rx_, duration, bytes_rx_);
    begin_time_rx_ = current_time;
    bytes_rx_ = 0;

    return speed_rx_;
}

//--------------------------------------------------------------------------------------------------
int UdpChannel::speedTx()
{
    TimePoint current_time = Clock::now();
    Milliseconds duration = std::chrono::duration_cast<Milliseconds>(current_time - begin_time_tx_);

    speed_tx_ = calculateSpeed(speed_tx_, duration, bytes_tx_);
    begin_time_tx_ = current_time;
    bytes_tx_ = 0;

    return speed_tx_;
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::init()
{
    static_assert(sizeof(Header) == 8, "Header must be 8 bytes without padding");

    write_queue_.reserve(kWriteQueueReservedSize);
    read_buffer_.reserve(kReadBufferReserveSize);
    write_buffer_.reserve(kWriteBufferReserveSize);
    decrypt_buffer_.reserve(kDecryptBufferReserveSize);

    connect(kcp_socket_, &KcpSocket::sig_connected, this, &UdpChannel::onKcpConnected);
    connect(kcp_socket_, &KcpSocket::sig_dataReceived, this, &UdpChannel::onKcpDataReceived);
    connect(kcp_socket_, &KcpSocket::sig_errorOccurred, this, [this]()
    {
        onErrorOccurred(FROM_HERE, std::error_code());
    });

    keep_alive_timer_ = new QTimer(this);
    keep_alive_timer_->setSingleShot(true);
    connect(keep_alive_timer_, &QTimer::timeout, this, &UdpChannel::onKeepAliveTimer);

    keep_alive_counter_.resize(sizeof(quint32));
    memset(keep_alive_counter_.data(), 0, keep_alive_counter_.size());
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::onErrorOccurred(const Location& location, const std::error_code& error_code)
{
    if (error_code)
        LOG(ERROR) << "Error occurred" << error_code << "from" << location.toString();
    else
        LOG(ERROR) << "Error occurred from" << location.toString();

    keep_alive_timer_->stop();
    kcp_socket_->close();

    connected_ = false;
    emit sig_errorOccurred();
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::onKcpConnected()
{
    connected_ = true;

    // Start keep-alive only if already unpaused.
    if (!paused_)
    {
        keep_alive_timer_type_ = KEEP_ALIVE_INTERVAL;
        keep_alive_timer_->start(kKeepAliveInterval);
    }

    // Emit sig_connected only for client-initiated connections (connectTo path).
    if (client_connect_)
    {
        client_connect_ = false;
        emit sig_connected();
    }
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::onKcpDataReceived(const char* data, int size)
{
    read_buffer_.append(data, size);
    addRxBytes(size);

    if (!paused_)
        parseMessages();
}

//--------------------------------------------------------------------------------------------------
bool UdpChannel::parseMessages()
{
    // Parse complete messages using offset tracking to avoid O(n) memmove on each call.
    while (true)
    {
        int available = read_buffer_.size() - read_offset_;
        int total_size = static_cast<int>(sizeof(Header));

        if (available < total_size)
            break;

        Header header;
        memcpy(&header, read_buffer_.constData() + read_offset_, sizeof(Header));

        if (header.length > kMaxMessageSize)
        {
            LOG(ERROR) << "Too big incoming message:" << header.length;
            onErrorOccurred(FROM_HERE, std::error_code());
            return false;
        }

        total_size += static_cast<int>(header.length);

        if (available < total_size)
            break;

        if (!onMessageReceived(read_offset_))
            return false;

        read_offset_ += total_size;

        if (paused_)
            break;
    }

    // Compact the buffer when consumed data exceeds threshold to limit memory waste.
    if (read_offset_ > kReadBufferCompactThreshold)
    {
        read_buffer_.remove(0, read_offset_);
        read_offset_ = 0;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool UdpChannel::onMessageReceived(int offset)
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
            return false;
        }
    }

    const char* payload_data = decryptor_ ? decrypt_buffer_.constData() : data;
    int payload_size = decryptor_ ? decrypt_buffer_.size() : size;

    if (header.type == USER_DATA)
    {
        emit sig_messageReceived(header.param1, QByteArray(payload_data, payload_size));
    }
    else if (header.type == KEEP_ALIVE)
    {
        if (header.param1 & KEEP_ALIVE_PING)
        {
            addWriteTask(KEEP_ALIVE, KEEP_ALIVE_PONG, QByteArray(payload_data, payload_size));
            return true;
        }

        if (payload_size != keep_alive_counter_.size())
        {
            LOG(ERROR) << "Invalid keep-alive payload size";
            onErrorOccurred(FROM_HERE, std::error_code());
            return false;
        }

        // Pong must contain the same data as ping.
        if (memcmp(payload_data, keep_alive_counter_.constData(), payload_size) != 0)
        {
            LOG(ERROR) << "Keep-alive counter mismatch";
            onErrorOccurred(FROM_HERE, std::error_code());
            return false;
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

    return true;
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

//--------------------------------------------------------------------------------------------------
void UdpChannel::addTxBytes(qint64 bytes_count)
{
    bytes_tx_ += bytes_count;
    total_tx_ += bytes_count;
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::addRxBytes(qint64 bytes_count)
{
    bytes_rx_ += bytes_count;
    total_rx_ += bytes_count;
}

} // namespace base
