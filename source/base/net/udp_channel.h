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

#ifndef BASE_NET_UDP_CHANNEL_H
#define BASE_NET_UDP_CHANNEL_H

#include <QObject>

#include <deque>
#include <memory>

#include "base/logging.h"
#include "base/net/anti_replay_window.h"

class QSocketNotifier;
class QTimer;

typedef struct _ENetAddress ENetAddress;
typedef struct _ENetHost ENetHost;
typedef struct _ENetPeer ENetPeer;
typedef struct _ENetPacket ENetPacket;

namespace base {

class DatagramDecryptor;
class DatagramEncryptor;
class Location;

class UdpChannel final : public QObject
{
    Q_OBJECT

public:
    explicit UdpChannel(QObject* parent = nullptr);
    ~UdpChannel() final;

    enum class Mode
    {
        UNKNOWN,
        READY_SOCKET,
        BIND,
        CONNECT
    };
    Q_ENUM(Mode)

    void bind(qintptr socket);
    void bind(quint16* port);
    void connectTo(const QString& address, quint16 port);
    void connectTo(qintptr socket, const QString& address, quint16 port);
    void setPeerAddress(const QString& address, quint16 port);
    void send(quint8 channel_id, const QByteArray& buffer);

    void setPaused(bool enable);

    void setEncryptor(std::unique_ptr<DatagramEncryptor> encryptor);
    void setDecryptor(std::unique_ptr<DatagramDecryptor> decryptor);

    qint64 pendingBytes() const;

    Mode mode() const { return mode_; }
    qint64 totalRx() const { return total_rx_; }
    qint64 totalTx() const { return total_tx_; }
    int speedRx();
    int speedTx();

signals:
    void sig_ready();
    void sig_errorOccurred();
    void sig_messageReceived(quint8 channel_id, const QByteArray& buffer);

protected:
    void timerEvent(QTimerEvent* event) final;

private:
    struct ENetHostDeleter { void operator()(ENetHost* host) const noexcept; };
    struct ENetPeerDeleter { void operator()(ENetPeer* peer) const noexcept; };
    struct ENetPacketDeleter { void operator()(ENetPacket* packet) const noexcept; };

    using ScopedENetHost = std::unique_ptr<ENetHost, ENetHostDeleter>;
    using ScopedENetPeer = std::unique_ptr<ENetPeer, ENetPeerDeleter>;
    using ScopedENetPacket = std::unique_ptr<ENetPacket, ENetPacketDeleter>;

    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Milliseconds = std::chrono::milliseconds;
    using Seconds = std::chrono::seconds;
    struct Header
    {
        quint64 counter;
        quint32 reserved1;
        quint8 type;
        quint8 reserved2;
        quint8 reserved3;
        quint8 reserved4;
    };

    enum MessageType { USER_DATA = 0 };

    void start();
    void close();
    void sendPunchHole(const ENetAddress& address);
    void processEvents();
    void onErrorOccurred(const Location& location);
    void addWriteTask(quint8 channel_id, const QByteArray& data);
    void onMessageReceived(quint8 channel_id, ScopedENetPacket packet);
    void onReadyCheck();
    void addTxBytes(qint64 bytes_count);
    void addRxBytes(qint64 bytes_count);
    ScopedENetPacket acquirePacket(qint64 size, quint32 flags);
    void releasePacket(ENetPacket* packet);

    QSocketNotifier* notifier_ = nullptr;
    ScopedENetHost host_;
    ScopedENetPeer peer_;
    int update_timer_id_ = 0;

    std::unique_ptr<DatagramEncryptor> encryptor_;
    std::unique_ptr<DatagramDecryptor> decryptor_;

    quint64 send_counter_ = 0;
    AntiReplayWindow replay_window_;
    qint64 pending_bytes_ = 0;

    std::deque<ScopedENetPacket> packet_pool_;
    std::deque<std::pair<quint8, ScopedENetPacket>> read_pending_;
    QByteArray decrypt_buffer_;

    bool connected_ = false;
    bool paused_ = true;

    Mode mode_ = Mode::UNKNOWN;

    qint64 total_tx_ = 0;
    qint64 total_rx_ = 0;

    TimePoint begin_time_tx_;
    qint64 bytes_tx_ = 0;
    int speed_tx_ = 0;

    TimePoint begin_time_rx_;
    qint64 bytes_rx_ = 0;
    int speed_rx_ = 0;

    LOG_DECLARE_CONTEXT(UdpChannel);
    Q_DISABLE_COPY_MOVE(UdpChannel)
};

} // namespace base

#endif // BASE_NET_UDP_CHANNEL_H
