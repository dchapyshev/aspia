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
#include "base/thread_local_pool.h"
#include "base/crypto/message_decryptor.h"
#include "base/crypto/message_encryptor.h"

namespace base {

namespace {

const quint32 kMaxMessageSize = 7 * 1024 * 1024; // 7 MB
const int kUpdateIntervalMs = 10;
const int kMaxPeers = 1;
const int kChannelCount = 255;
const int kPoolReservedSize = 64;
const int kMtu = 1200;

// ENet hot-path allocations (per packet, x64 sizes + 16-byte AllocHeader):
//   ENetPacket:          ~48 bytes  -> 64  total -> bucket[0] (64)
//   ENetAcknowledgement: ~68 bytes  -> 84  total -> bucket[1] (128)
//   ENetOutgoingCommand: ~96 bytes  -> 112 total -> bucket[1] (128)
//   ENetIncomingCommand: ~92 bytes  -> 108 total -> bucket[1] (128)
//   Packet data buffers: variable          total -> bucket[2..5]
//
// Cold-path allocations (ENetHost ~12KB, ENetPeer[] ~400*N, ENetChannel[] ~76*N)
// are too large and infrequent for pooling — they fall through to std::malloc.
constexpr size_t kENetBucketSizes[] = { 64, 128, 256, 512, 1024, 2048 };

using ENetPool = ThreadLocalPool<std::size(kENetBucketSizes), 64>;
thread_local ENetPool tls_enet_pool(kENetBucketSizes);

//--------------------------------------------------------------------------------------------------
int calculateSpeed(int last_speed, const std::chrono::milliseconds& duration, qint64 bytes)
{
    static const double kAlpha = 0.1;
    return static_cast<int>(
        (kAlpha * ((1000.0 / static_cast<double>(duration.count())) * static_cast<double>(bytes))) +
        ((1.0 - kAlpha) * static_cast<double>(last_speed)));
}

//--------------------------------------------------------------------------------------------------
ENetHost* createHost(const ENetAddress* address)
{
    ENetHost* host = enet_host_create(address, kMaxPeers, kChannelCount, 0, 0);
    if (host)
        host->mtu = kMtu;
    return host;
}

//--------------------------------------------------------------------------------------------------
void initializeEnetWithPool()
{
    static struct Initializer
    {
        Initializer()
        {
            ENetCallbacks callbacks;
            callbacks.malloc = [](size_t size) { return tls_enet_pool.allocate(size); };
            callbacks.free = [](void* memory) { tls_enet_pool.deallocate(memory); };
            callbacks.no_memory = []() { LOG(FATAL) << "ENet: out of memory"; };

            int ret = enet_initialize_with_callbacks(ENET_VERSION, &callbacks);
            CHECK(ret == 0) << "Failed to initialize ENet with custom allocator";
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
    initializeEnetWithPool();
    connect(notifier_, &QSocketNotifier::activated, this, &UdpChannel::processEvents);
}

//--------------------------------------------------------------------------------------------------
UdpChannel::~UdpChannel()
{
    close();
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::bind(qintptr socket)
{
    if (host_ || mode_ != Mode::UNKNOWN)
        return;

    mode_ = Mode::READY_SOCKET;

    ENetAddress fake_address;
    fake_address.host = ENET_HOST_ANY;
    fake_address.port = 0;

    host_.reset(createHost(&fake_address));
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

    start();
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::bind(quint16* port)
{
    DCHECK(port);

    if (host_ || mode_ != Mode::UNKNOWN)
        return;

    mode_ = Mode::BIND;

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = *port;

    host_.reset(createHost(&address));
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

    start();
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::connectTo(const QString& address, quint16 port)
{
    if (host_ || mode_ != Mode::UNKNOWN)
        return;

    mode_ = Mode::CONNECT;

    host_.reset(createHost(nullptr));
    if (!host_)
    {
        LOG(ERROR) << "Failed to create ENet host";
        onErrorOccurred(FROM_HERE);
        return;
    }

    ENetAddress enet_address;
    enet_address.port = port;

    if (enet_address_set_host(&enet_address, address.toLocal8Bit().constData()) != 0)
    {
        LOG(ERROR) << "Failed to resolve address:" << address;
        onErrorOccurred(FROM_HERE);
        return;
    }

    peer_.reset(enet_host_connect(host_.get(), &enet_address, kChannelCount, 0));
    if (!peer_)
    {
        LOG(ERROR) << "Failed to initiate ENet connection";
        onErrorOccurred(FROM_HERE);
        return;
    }

    enet_host_flush(host_.get());
    start();
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::connectTo(qintptr socket, const QString& address, quint16 port)
{
    if (host_ || mode_ != Mode::UNKNOWN)
        return;

    mode_ = Mode::READY_SOCKET;

    ENetAddress fake_address;
    fake_address.host = ENET_HOST_ANY;
    fake_address.port = 0;

    host_.reset(createHost(&fake_address));
    if (!host_)
    {
        LOG(ERROR) << "Unable to create ENet host";
        onErrorOccurred(FROM_HERE);
        return;
    }

    enet_socket_destroy(host_->socket);

    ENetAddress bound_address;
    if (enet_socket_get_address(socket, &bound_address) < 0)
    {
        LOG(ERROR) << "Unable to get socket address";
        onErrorOccurred(FROM_HERE);
        return;
    }

    host_->socket = socket;
    host_->address = bound_address;

    // Send punch hole packets to open our NAT for the peer.
    sendPunchHole(bound_address);

    // Initiate ENet connection to peer. ENet retries automatically, so even if the peer's NAT hasn't
    // opened yet, subsequent attempts will succeed after the peer sends its punch hole packets.
    ENetAddress enet_address;
    enet_address.port = port;

    if (enet_address_set_host(&enet_address, address.toLocal8Bit().constData()) != 0)
    {
        LOG(ERROR) << "enet_address_set_host failed";
        onErrorOccurred(FROM_HERE);
        return;
    }

    LOG(INFO) << "Initiating ENet connection to peer:" << address << ":" << port;

    peer_.reset(enet_host_connect(host_.get(), &enet_address, kChannelCount, 0));
    if (!peer_)
    {
        LOG(ERROR) << "Failed to initiate ENet connection";
        onErrorOccurred(FROM_HERE);
        return;
    }

    enet_host_flush(host_.get());
    start();
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::setPeerAddress(const QString& address, quint16 port)
{
    if (!host_)
    {
        LOG(ERROR) << "Host not created";
        return;
    }

    ENetAddress enet_address;
    enet_address.port = port;

    if (enet_address_set_host(&enet_address, address.toLocal8Bit().constData()) != 0)
    {
        LOG(ERROR) << "enet_address_set_host failed";
        return;
    }

    sendPunchHole(enet_address);
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

    paused_ = enable;
    if (paused_)
        return;

    for (auto& task : read_pending_)
        onMessageReceived(task.first, std::move(task.second));
    read_pending_.clear();
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

    ScopedENetPacket packet = acquirePacket(sizeof(Header) + target_data_size, ENET_PACKET_FLAG_RELIABLE);
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

    Task& task = write_queue_.front();
    ScopedENetPacket packet = std::move(task.second);
    quint8 channel_id = task.first;

    addTxBytes(packet->dataLength);
    write_queue_.pop_front();

    // This callback is called when a packet is removed.
    packet->userData = this;
    packet->freeCallback = [](ENetPacket* packet)
    {
        UdpChannel* self = reinterpret_cast<UdpChannel*>(packet->userData);

        // Call releasePacket to take back the allocated memory from this packet.
        self->releasePacket(packet);

        // Since the packet has been removed, the sending task is complete.
        // Let's start the next sending task.
        self->doWrite();
    };

    if (enet_peer_send(peer_.get(), channel_id, packet.release()) != 0)
    {
        LOG(ERROR) << "enet_peer_send failed";
        onErrorOccurred(FROM_HERE);
        return;
    }

    enet_host_flush(host_.get());
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
qint64 UdpChannel::pendingBytes() const
{
    qint64 result = 0;

    for (const auto& task : std::as_const(write_queue_))
        result += task.second->dataLength;

    return result;
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
void UdpChannel::start()
{
    update_timer_id_ = startTimer(kUpdateIntervalMs, Qt::PreciseTimer);
    notifier_->setSocket(host_->socket);
    notifier_->setEnabled(true);
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::close()
{
    if (update_timer_id_ != 0)
    {
        killTimer(update_timer_id_);
        update_timer_id_ = 0;
    }
    notifier_->setEnabled(false);
    peer_.reset();
    host_.reset();
    packet_pool_.clear();
    connected_ = false;
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::sendPunchHole(const ENetAddress& address)
{
    static const char kData[] = "PUNCH_HOLE_PACKET";
    static const int kCount = 5;

    ENetBuffer buffer;
    buffer.data = const_cast<char*>(kData);
    buffer.dataLength = sizeof(kData);

    for (int i = 0; i < kCount; ++i)
        enet_socket_send(host_->socket, &address, &buffer, 1);
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
                LOG(INFO) << "Peer connected";
                if (!peer_)
                    peer_.reset(event.peer);

                const int kSocketBufferSize = 2 * 1024 * 1024;

                // Should improve resilience to traffic spikes.
                enet_socket_set_option(host_->socket, ENET_SOCKOPT_SNDBUF, kSocketBufferSize);
                enet_socket_set_option(host_->socket, ENET_SOCKOPT_RCVBUF, kSocketBufferSize);

                // By default is 5s before the first retransmit. Reduce this to 3s.
                enet_peer_timeout(peer_.get(), ENET_PEER_TIMEOUT_LIMIT, 3000, 10000);

                // By default interval is 5s, we reduce it to 3s to increase the speed of
                // adaptation to changes in network quality.
                enet_peer_throttle_configure(peer_.get(), 3000, 2, 2);

                connected_ = true;
                onReadyCheck();
            }
            break;

            case ENET_EVENT_TYPE_RECEIVE:
            {
                onMessageReceived(event.channelID, ScopedENetPacket(event.packet));
                // Host was destroyed during parseMessages (error path).
                if (!host_)
                    return;
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
        onErrorOccurred(FROM_HERE);
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
void UdpChannel::onMessageReceived(quint8 channel_id, ScopedENetPacket packet)
{
    if (paused_)
    {
        read_pending_.emplace_back(channel_id, std::move(packet));
        return;
    }

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

    qint64 size = decryptor_->decryptedDataSize(packet->dataLength - sizeof(Header));
    if (decrypt_buffer_.capacity() < size)
        decrypt_buffer_.reserve(size);

    decrypt_buffer_.resize(size);

    if (!decryptor_->decrypt(packet->data + sizeof(Header), packet->dataLength - sizeof(Header),
        &header, sizeof(Header), decrypt_buffer_.data()))
    {
        LOG(ERROR) << "Failed to decrypt incoming message";
        onErrorOccurred(FROM_HERE);
        return;
    }

    if (header.type == USER_DATA)
        emit sig_messageReceived(channel_id, decrypt_buffer_);
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::onReadyCheck()
{
    if (!connected_ || !encryptor_ || !decryptor_)
        return;

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
void UdpChannel::releasePacket(ENetPacket* release_packet)
{
    if (packet_pool_.size() >= kPoolReservedSize)
        return;

    // Create a new package without allocating memory with using the data of the released packet.
    ScopedENetPacket packet(enet_packet_create(
        release_packet->data, release_packet->dataLength, ENET_PACKET_FLAG_NO_ALLOCATE));

    // Remove the flags, now the new packet owns the data.
    packet->flags = 0;

    // Old packet no longer contains data.
    release_packet->data = nullptr;
    release_packet->dataLength = 0;

    packet_pool_.emplace_back(std::move(packet));
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
