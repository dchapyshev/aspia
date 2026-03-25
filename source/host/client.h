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

#ifndef HOST_CLIENT_H
#define HOST_CLIENT_H

#include <QObject>
#include <QVersionNumber>

#include <chrono>
#include <optional>

#include "base/crypto/key_pair.h"
#include "base/net/tcp_channel.h"
#include "proto/peer.h"

class QTimer;

namespace base {
class StunPeer;
class UdpChannel;
} // namespace base

namespace host {

class Client : public QObject
{
    Q_OBJECT

public:
    Client(base::TcpChannel* tcp_channel, QObject* parent);
    virtual ~Client();

    void start(bool direct, const QString& stun_host, quint16 stun_port, bool peer_equals);

    quint32 clientId() const;
    proto::peer::SessionType sessionType() const;
    QVersionNumber version() const;
    QString osName() const;
    QString computerName() const;
    QString displayName() const;
    QString architecture() const;
    QString userName() const;
    qint64 pendingBytes() const;

    enum class UdpConnectPhase
    {
        NONE,
        DIRECT_LAN,   // Bind in LAN (peer_address_equals == true).
        HOLE_PUNCHING // STUN-based hole punching (up to kMaxHolePunchingAttempts).
    };
    Q_ENUM(UdpConnectPhase)

    qint64 bandwidth() const { return bandwidth_; }

signals:
    void sig_started();
    void sig_finished();
    void sig_connectionChanged();

protected:
    void send(quint8 channel_id, const QByteArray& buffer);

    virtual void onStart() = 0;
    virtual void onMessage(quint8 channel_id, const QByteArray& buffer) = 0;
    virtual void onBandwidthChanged(qint64 bandwidth) { /* Nothing */ };

private slots:
    void onTcpErrorOccurred(base::TcpChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 tcp_channel_id, const QByteArray& buffer);

    void onUdpReady();
    void onUdpErrorOccurred();
    void onUdpMessageReceived(quint8 udp_channel_id, const QByteArray& buffer);

private:
    void startUdpHolePunching();
    void startDirectUdp(qintptr socket, const QString& address, quint16 port);
    void readDirectUdpReply(const proto::peer::DirectUdpReply& reply);
    void startBandwidthProbing();
    void sendBandwidthProbe();
    void onBandwidthProbeAck();

    base::TcpChannel* tcp_channel_ = nullptr;
    base::UdpChannel* udp_channel_ = nullptr;
    bool udp_ready_ = false;

    struct PendingUdp
    {
        base::KeyPair key_pair;
        QByteArray iv;
    };

    base::StunPeer* stun_peer_ = nullptr;
    std::optional<PendingUdp> pending_udp_context_;

    UdpConnectPhase udp_phase_ = UdpConnectPhase::NONE;
    int hole_punching_attempt_ = 0;
    bool direct_ = false;
    bool peer_equals_ = false;
    QString stun_host_;
    quint16 stun_port_ = 0;

    // Bandwidth probing.
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Milliseconds = std::chrono::milliseconds;

    QTimer* probe_timer_ = nullptr;
    TimePoint last_send_time_;
    TimePoint probe_send_time_;
    bool probe_pending_ = false;
    qint64 bandwidth_ = 0;

    Q_DISABLE_COPY_MOVE(Client)
};

} // namespace host

#endif // HOST_CLIENT_H
