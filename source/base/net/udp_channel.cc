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

#include <QSocketNotifier>
#include <QTimer>
#include <QTimerEvent>

#include <enet/enet.h>

#include "base/location.h"
#include "base/logging.h"
#include "base/crypto/message_decryptor.h"
#include "base/crypto/message_encryptor.h"

namespace base {

namespace {

const quint32 kMaxMessageSize = 7 * 1024 * 1024; // 7 MB
const int kDecryptBufferReserveSize = 2 * 1024 * 1024;
const int kUpdateIntervalMs = 10;
const int kMaxPeers = 1;
const int kChannelCount = 256;
const int kPoolReservedSize = 64;

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
      notifier_(new QSocketNotifier(QSocketNotifier::Read, this))
{
    ensureEnetInitialized();

    decrypt_buffer_.reserve(kDecryptBufferReserveSize);

    connect(notifier_, &QSocketNotifier::activated, this, &UdpChannel::processEvents);
}

//--------------------------------------------------------------------------------------------------
UdpChannel::~UdpChannel()
{
    close();
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::setReadySocket(qintptr socket)
{
    ENetAddress fake_address;
    fake_address.host = ENET_HOST_ANY;
    fake_address.port = 0;

    host_.reset(enet_host_create(&fake_address, kMaxPeers, kChannelCount, 0, 0));
    if (!host_)
    {
        LOG(ERROR) << "Unable to create ENet host";
        onErrorOccurred(FROM_HERE);
        return;
    }

    enet_socket_destroy(host_->socket);

    ENetAddress address;
    if (enet_socket_get_address(socket, &address) < 0)
    {
        LOG(ERROR) << "Unable to get socket address";
        onErrorOccurred(FROM_HERE);
        return;
    }

    host_->socket = socket;
    host_->address = address;

    setUpdateEnabled(true);
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::bind(quint16* port)
{
    if (host_)
        return;

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = *port;

    host_.reset(enet_host_create(&address, kMaxPeers, kChannelCount, 0, 0));
    if (!host_)
    {
        LOG(ERROR) << "Unable to create ENet host on port" << *port;
        onErrorOccurred(FROM_HERE);
        return;
    }

    if (!*port)
    {
        // Retrieve the actual port assigned by the OS.
        ENetAddress bound_address;
        if (enet_socket_get_address(host_->socket, &bound_address) != 0)
        {
            LOG(ERROR) << "Unable to get bound port";
            onErrorOccurred(FROM_HERE);
            return;
        }

        *port = bound_address.port;
    }

    setUpdateEnabled(true);
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::connectTo(const QString& address, quint16 port)
{
    if (host_)
        return;

    host_.reset(enet_host_create(nullptr, kMaxPeers, kChannelCount, 0, 0));
    if (!host_)
    {
        LOG(ERROR) << "Failed to create ENet host";
        emit sig_errorOccurred();
        return;
    }

    ENetAddress enet_address;
    enet_address.port = port;

    if (enet_address_set_host(&enet_address, address.toLocal8Bit().constData()) != 0)
    {
        LOG(ERROR) << "Failed to resolve address:" << address;
        emit sig_errorOccurred();
        return;
    }

    peer_.reset(enet_host_connect(host_.get(), &enet_address, kChannelCount, 0));
    if (!peer_)
    {
        LOG(ERROR) << "Failed to initiate ENet connection";
        emit sig_errorOccurred();
        return;
    }

    enet_host_flush(host_.get());
    setUpdateEnabled(true);
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::send(quint8 channel_id, const QByteArray& buffer)
{
    addWriteTask(channel_id, buffer);
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::setPaused(bool enable)
{
    if (paused_ == enable)
        return;

    setUpdateEnabled(!enable);
    paused_ = enable;
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::addWriteTask(quint8 channel_id, const QByteArray& data)
{
    if (!encryptor_)
    {
        onErrorOccurred(FROM_HERE);
        return;
    }

    qint64 target_data_size = encryptor_->encryptedDataSize(data.size());
    if (target_data_size > kMaxMessageSize)
    {
        LOG(ERROR) << "Too big outgoing message:" << target_data_size;
        onErrorOccurred(FROM_HERE);
        return;
    }

    ScopedENetPacket packet = acquirePacket(target_data_size, ENET_PACKET_FLAG_RELIABLE);
    if (!packet)
    {
        onErrorOccurred(FROM_HERE);
        return;
    }

    Header header;
    header.type = USER_DATA;
    header.reserved1 = 0;
    header.reserved2 = 0;
    header.reserved3 = 0;

    memcpy(packet->data, &header, sizeof(Header));

    if (!encryptor_->encrypt(data.constData(), data.size(), &header, sizeof(Header),
        packet->data + sizeof(Header)))
    {
        LOG(ERROR) << "Failed to encrypt outgoing message";
        onErrorOccurred(FROM_HERE);
        return;
    }

    bool schedule_write = write_queue_.empty();

    write_queue_.emplace_back(channel_id, std::move(packet));

    if (schedule_write)
        doWrite();
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::doWrite()
{
    if (write_queue_.empty())
        return;

    WriteTask& task = write_queue_.front();
    ScopedENetPacket packet = std::move(task.second);
    quint8 channel_id = task.first;

    addTxBytes(packet->dataLength);
    write_queue_.pop_front();

    packet->userData = this;
    packet->freeCallback = [](ENetPacket* packet)
    {
        UdpChannel* self = reinterpret_cast<UdpChannel*>(packet->userData);
        self->releasePacket(packet);
        self->doWrite();
    };

    if (enet_peer_send(peer_.get(), channel_id, packet.release()) != 0)
    {
        LOG(ERROR) << "enet_peer_send failed";
        emit sig_errorOccurred();
        return;
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
    if (event->timerId() == update_timer_id_)
        processEvents();
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::close()
{
    setUpdateEnabled(false);
    peer_.reset();
    host_.reset();
    packet_pool_.clear();
    connected_ = false;
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::processEvents()
{
    if (!host_)
        return;

    ENetEvent event;
    int result;

    while ((result = enet_host_service(host_.get(), &event, 0)) > 0)
    {
        switch (event.type)
        {
            case ENET_EVENT_TYPE_CONNECT:
            {
                LOG(INFO) << "ENet peer connected";
                if (!peer_)
                    peer_.reset(event.peer);
                connected_ = true;
                onReadyCheck();
            }
            break;

            case ENET_EVENT_TYPE_RECEIVE:
            {
                onMessageReceived(event.channelID, event.packet);
                enet_packet_destroy(event.packet);

                if (!host_)
                {
                    // Host was destroyed during parseMessages (error path).
                    return;
                }
            }
            break;

            case ENET_EVENT_TYPE_DISCONNECT:
                onErrorOccurred(FROM_HERE);
                return;

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
void UdpChannel::onErrorOccurred(const Location& location)
{
    LOG(ERROR) << "Error occurred from" << location.toString();
    close();
    emit sig_errorOccurred();
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::onMessageReceived(quint8 channel_id, ENetPacket* packet)
{
    if (!decryptor_)
    {
        onErrorOccurred(FROM_HERE);
        return;
    }

    addRxBytes(packet->dataLength);

    if (packet->dataLength > kMaxMessageSize)
    {
        LOG(ERROR) << "Too big incoming message:" << packet->dataLength;
        onErrorOccurred(FROM_HERE);
        return;
    }

    Header header;
    memcpy(&header, packet->data, sizeof(Header));

    qint64 size = decryptor_->decryptedDataSize(packet->dataLength);
    if (decrypt_buffer_.capacity() < size)
        decrypt_buffer_.reserve(size);

    decrypt_buffer_.resize(size);

    if (!decryptor_->decrypt(packet->data, packet->dataLength, &header, sizeof(Header), decrypt_buffer_.data()))
    {
        LOG(ERROR) << "Failed to decrypt incoming message";
        onErrorOccurred(FROM_HERE);
        return;
    }

    if (header.type == USER_DATA)
        emit sig_messageReceived(channel_id, decrypt_buffer_);
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::setUpdateEnabled(bool enable)
{
    if (enable && update_timer_id_ == 0)
    {
        update_timer_id_ = startTimer(kUpdateIntervalMs, Qt::PreciseTimer);
    }
    else if (!enable && update_timer_id_ != 0)
    {
        killTimer(update_timer_id_);
        update_timer_id_ = 0;
    }

    if (notifier_->isEnabled() != enable)
        notifier_->setEnabled(enable);
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::onReadyCheck()
{
    if (!connected_ || !encryptor_ || !decryptor_)
        return;

    // The channel is created in a paused state. Once the connection is established, we stop the
    // update. The owner must handle signal sig_ready and call method setPaused(false) to resume
    // receiving messages.
    setUpdateEnabled(false);

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

//--------------------------------------------------------------------------------------------------
UdpChannel::ScopedENetPacket UdpChannel::acquirePacket(qint64 size, quint32 flags)
{
    if (!packet_pool_.empty())
    {
        ScopedENetPacket packet = std::move(packet_pool_.front());
        packet_pool_.pop_front();

        enet_packet_resize(packet.get(), size);
        packet->flags = flags;

        return packet;
    }

    ScopedENetPacket packet(enet_packet_create(nullptr, size, flags));
    if (!packet)
    {
        LOG(ERROR) << "Failed to create ENet packet";
        return nullptr;
    }

    return packet;
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::releasePacket(ENetPacket* packet)
{
    if (packet_pool_.size() >= kPoolReservedSize)
    {
        enet_packet_destroy(packet);
        return;
    }

    packet_pool_.emplace_back(packet);
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::ENetHostDeleter::operator()(ENetHost* host) const noexcept
{
    if (host)
        enet_host_destroy(host);
}

void UdpChannel::ENetPeerDeleter::operator()(ENetPeer* peer) const noexcept
{
    if (peer)
        enet_peer_reset(peer);
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::ENetPacketDeleter::operator()(ENetPacket* packet) const noexcept
{
    if (packet)
        enet_packet_destroy(packet);
}

} // namespace base
