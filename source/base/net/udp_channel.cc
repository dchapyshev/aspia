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
#include <QTimerEvent>

#include <enet/enet.h>

#include "base/location.h"
#include "base/logging.h"
#include "base/crypto/large_number_increment.h"
#include "base/crypto/message_decryptor.h"
#include "base/crypto/message_encryptor.h"

namespace base {

namespace {

const std::chrono::seconds kKeepAliveInterval { 30 };
const std::chrono::seconds kKeepAliveTimeout { 30 };
const quint32 kMaxMessageSize = 7 * 1024 * 1024; // 7 MB
const int kWriteQueueReservedSize = 128;
const int kReadBufferReserveSize = 1 * 1024 * 1024;
const int kWriteBufferReserveSize = 2 * 1024 * 1024;
const int kDecryptBufferReserveSize = 2 * 1024 * 1024;
const int kReadBufferCompactThreshold = 1 * 1024 * 1024;

const int kUpdateIntervalMs = 10;
const int kMaxPeers = 1;
const int kChannelCount = 256;

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

//--------------------------------------------------------------------------------------------------
void ensureEnetInitialized()
{
    static struct Initializer
    {
        Initializer()
        {
            int ret = enet_initialize();
            CHECK(ret == 0) << "Failed to initialize ENet";
        }
        ~Initializer() { enet_deinitialize(); }
    } initializer;
}

} // namespace

//--------------------------------------------------------------------------------------------------
UdpChannel::UdpChannel(QObject* parent)
    : QObject(parent),
      keep_alive_timer_(new QTimer(this))
{
    ensureEnetInitialized();
}

//--------------------------------------------------------------------------------------------------
UdpChannel::~UdpChannel()
{
    close();
}

//--------------------------------------------------------------------------------------------------
bool UdpChannel::setReadySocket(qintptr socket)
{
    ENetAddress fake_address;
    fake_address.host = ENET_HOST_ANY;
    fake_address.port = 0;

    ENetHost* host = enet_host_create(&fake_address, kMaxPeers, kChannelCount, 0, 0);
    if (!host)
    {
        LOG(ERROR) << "Unable to create ENet host";
        enet_socket_destroy(socket);
        return false;
    }

    enet_socket_destroy(host->socket);

    ENetAddress address;
    if (enet_socket_get_address(socket, &address) < 0)
    {
        LOG(ERROR) << "Unable to get socket address";
        enet_socket_destroy(socket);
        return false;
    }

    host->socket = socket;
    host->address = address;

    init(host);
    return true;
}

//--------------------------------------------------------------------------------------------------
bool UdpChannel::bind(quint16* port)
{
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = *port;

    ENetHost* host = enet_host_create(&address, kMaxPeers, kChannelCount, 0, 0);
    if (!host)
    {
        LOG(ERROR) << "Unable to create ENet host on port" << *port;
        return false;
    }

    if (!*port)
    {
        // Retrieve the actual port assigned by the OS.
        ENetAddress bound_address;
        if (enet_socket_get_address(host->socket, &bound_address) != 0)
        {
            LOG(ERROR) << "Unable to get bound port";
            enet_host_destroy(host);
            return false;
        }

        *port = bound_address.port;
    }

    init(host);
    return true;
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::connectTo(const QString& address, quint16 port)
{
    if (!host_)
    {
        ENetHost* host = enet_host_create(nullptr, kMaxPeers, kChannelCount, 0, 0);
        if (!host)
        {
            LOG(ERROR) << "Failed to create ENet host";
            emit sig_errorOccurred();
            return;
        }

        init(host);
    }

    ENetAddress enet_address;
    enet_address.port = port;

    if (enet_address_set_host(&enet_address, address.toLocal8Bit().constData()) != 0)
    {
        LOG(ERROR) << "Failed to resolve address:" << address;
        emit sig_errorOccurred();
        return;
    }

    peer_.reset(enet_host_connect(host_, &enet_address, kChannelCount, 0));
    if (!peer_)
    {
        LOG(ERROR) << "Failed to initiate ENet connection";
        emit sig_errorOccurred();
        return;
    }

    enet_host_flush(host_);
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::send(quint8 channel_id, const QByteArray& buffer)
{
    addWriteTask(USER_DATA, channel_id, buffer);
}

//--------------------------------------------------------------------------------------------------
bool UdpChannel::isReady() const
{
    return enet_ready_ && isEncrypted();
}

//--------------------------------------------------------------------------------------------------
bool UdpChannel::isEncrypted() const
{
    return encryptor_ && decryptor_;
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

    if (enet_ready_)
    {
        // Start keep-alive only when we know the peer's address.
        keep_alive_timer_type_ = KEEP_ALIVE_INTERVAL;
        keep_alive_timer_->start(kKeepAliveInterval);
    }

    // Process any data that was accumulated while paused.
    parseMessages();
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
    if (write_queue_.isEmpty())
        return;

    if (!encryptor_)
    {
        onErrorOccurred(FROM_HERE, std::error_code());
        return;
    }

    while (!write_queue_.isEmpty())
    {
        const WriteTask& task = write_queue_.front();
        const QByteArray& source_buffer = task.data();

        qint64 target_data_size = encryptor_->encryptedDataSize(source_buffer.size());
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

        if (!encryptor_->encrypt(source_buffer.constData(), source_buffer.size(),
            &header, sizeof(Header), write_buffer_.data() + sizeof(Header)))
        {
            LOG(ERROR) << "Failed to encrypt outgoing message";
            onErrorOccurred(FROM_HERE, std::error_code());
            return;
        }

        addTxBytes(write_buffer_.size());

        ENetPacket* packet = enet_packet_create(write_buffer_.constData(), write_buffer_.size(), ENET_PACKET_FLAG_RELIABLE);
        if (!packet)
        {
            LOG(ERROR) << "Failed to create ENet packet";
            emit sig_errorOccurred();
            return;
        }

        if (enet_peer_send(peer_, 0, packet) != 0)
        {
            LOG(ERROR) << "enet_peer_send failed";
            enet_packet_destroy(packet);
            emit sig_errorOccurred();
            return;
        }

        enet_host_flush(host_);
        write_queue_.pop_front();
    }
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::setEncryptor(std::unique_ptr<MessageEncryptor> encryptor)
{
    encryptor_ = std::move(encryptor);
    onReadyCheck();
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::setDecryptor(std::unique_ptr<MessageDecryptor> decryptor)
{
    decryptor_ = std::move(decryptor);
    onReadyCheck();
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
void UdpChannel::timerEvent(QTimerEvent* event)
{
    if (event->timerId() != update_timer_id_)
        return;

    processEvents();
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::init(ENetHost* host)
{
    static_assert(sizeof(Header) == 8, "Header must be 8 bytes without padding");

    write_queue_.reserve(kWriteQueueReservedSize);
    read_buffer_.reserve(kReadBufferReserveSize);
    write_buffer_.reserve(kWriteBufferReserveSize);
    decrypt_buffer_.reserve(kDecryptBufferReserveSize);

    host_.reset(host);
    update_timer_id_ = startTimer(kUpdateIntervalMs, Qt::PreciseTimer);

    keep_alive_timer_->setSingleShot(true);
    connect(keep_alive_timer_, &QTimer::timeout, this, &UdpChannel::onKeepAliveTimer);

    keep_alive_counter_.resize(sizeof(quint32));
    memset(keep_alive_counter_.data(), 0, keep_alive_counter_.size());
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::close()
{
    keep_alive_timer_->stop();

    if (update_timer_id_ != 0)
    {
        killTimer(update_timer_id_);
        update_timer_id_ = 0;
    }

    peer_.reset();
    host_.reset();

    enet_ready_ = false;
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::processEvents()
{
    if (!host_)
        return;

    ENetEvent event;
    int result;

    while ((result = enet_host_service(host_, &event, 0)) > 0)
    {
        switch (event.type)
        {
            case ENET_EVENT_TYPE_CONNECT:
            {
                LOG(INFO) << "ENet peer connected";
                peer_.reset(event.peer);
                enet_ready_ = true;
                onReadyCheck();
            }
            break;

            case ENET_EVENT_TYPE_RECEIVE:
            {
                addRxBytes(event.packet->dataLength);
                read_buffer_.append(reinterpret_cast<const char*>(event.packet->data),
                                    static_cast<int>(event.packet->dataLength));
                parseMessages();
                enet_packet_destroy(event.packet);
            }
            break;

            case ENET_EVENT_TYPE_DISCONNECT:
                LOG(INFO) << "ENet peer disconnected";
                peer_.reset();
                enet_ready_ = false;
                emit sig_errorOccurred();
                return; // Host may be destroyed by the slot, stop processing.

            case ENET_EVENT_TYPE_NONE:
                break;
        }
    }

    if (result < 0)
    {
        LOG(ERROR) << "enet_host_service failed";
        emit sig_errorOccurred();
    }
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::onErrorOccurred(const Location& location, const std::error_code& error_code)
{
    if (error_code)
        LOG(ERROR) << "Error occurred" << error_code << "from" << location.toString();
    else
        LOG(ERROR) << "Error occurred from" << location.toString();

    close();
    emit sig_errorOccurred();
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::parseMessages()
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
            return;
        }

        total_size += static_cast<int>(header.length);

        if (available < total_size)
            break;

        if (!onMessageReceived(read_offset_))
            return;

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
}

//--------------------------------------------------------------------------------------------------
bool UdpChannel::onMessageReceived(int offset)
{
    Header header;
    memcpy(&header, read_buffer_.constData() + offset, sizeof(Header));

    const char* data = read_buffer_.constData() + offset + sizeof(Header);
    int size = static_cast<int>(header.length);

    if (!decryptor_)
    {
        onErrorOccurred(FROM_HERE, std::error_code());
        return false;
    }

    resizeBuffer(&decrypt_buffer_, decryptor_->decryptedDataSize(size));

    if (!decryptor_->decrypt(data, size, &header, sizeof(Header), decrypt_buffer_.data()))
    {
        LOG(ERROR) << "Failed to decrypt incoming message";
        onErrorOccurred(FROM_HERE, std::error_code());
        return false;
    }

    if (header.type == USER_DATA)
    {
        emit sig_messageReceived(header.param1, decrypt_buffer_);
    }
    else if (header.type == KEEP_ALIVE)
    {
        if (header.param1 & KEEP_ALIVE_PING)
        {
            addWriteTask(KEEP_ALIVE, KEEP_ALIVE_PONG, decrypt_buffer_);
            return true;
        }

        // Pong must contain the same data as ping.
        if (decrypt_buffer_ != keep_alive_counter_)
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
void UdpChannel::onReadyCheck()
{
    if (!enet_ready_ || !isEncrypted())
        return;

    // Start keep-alive only if already unpaused.
    if (!paused_)
    {
        keep_alive_timer_type_ = KEEP_ALIVE_INTERVAL;
        keep_alive_timer_->start(kKeepAliveInterval);
    }

    LOG(INFO) << "UDP channel is ready";
    emit sig_ready();
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
