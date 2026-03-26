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

#include "base/logging.h"
#include "base/crypto/key_pair.h"
#include "base/net/tcp_channel.h"
#include "proto/peer.h"

class QTimer;

namespace base {
class Location;
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

    enum Feature
    {
        FEATURE_NONE      = 0,
        FEATURE_UDP       = 1,
        FEATURE_BANDWIDTH = 2
    };
    Q_DECLARE_FLAGS(Features, Feature)

    void start(bool direct, const QString& stun_host, quint16 stun_port, bool peer_equals);
    void setFeature(Feature feature, bool enable);

    quint32 clientId() const;
    proto::peer::SessionType sessionType() const;
    QVersionNumber version() const;
    QString osName() const;
    QString computerName() const;
    QString displayName() const;
    QString architecture() const;
    QString userName() const;
    qint64 pendingBytes() const;
    qint64 bandwidth() const;

    enum class UdpConnectPhase
    {
        NONE,
        DIRECT_LAN,   // Bind in LAN (peer_address_equals == true).
        HOLE_PUNCHING // STUN-based hole punching (up to kMaxHolePunchingAttempts).
    };
    Q_ENUM(UdpConnectPhase)

    enum class UdpState
    {
        DISCONNECTED,
        CONNECTED,
        PROBED,
        READY
    };
    Q_ENUM(UdpState)

signals:
    void sig_started();
    void sig_finished();
    void sig_connectionChanged();

protected:
    LOG_DECLARE_CONTEXT(Client);

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
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Milliseconds = std::chrono::milliseconds;

    void startUdpHolePunching();
    void startDirectUdp(qintptr socket, const QString& address, quint16 port);
    void readDirectUdpReply(const proto::peer::DirectUdpReply& reply);
    void startBandwidthProbing();
    void sendTcpBandwidthProbe(const TimePoint& time);
    void sendUdpBandwidthProbe(const TimePoint& time);
    void onTcpBandwidthProbeAck();
    void onUdpBandwidthProbeAck();
    void checkBandwidth();
    void setUdpState(const base::Location& location, UdpState state);
    static QByteArray makeBandwidthProbeData();

    Features features_ = FEATURE_NONE;
    base::TcpChannel* tcp_channel_ = nullptr;
    base::UdpChannel* udp_channel_ = nullptr;
    UdpState udp_state_ = UdpState::DISCONNECTED;

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

    struct BandwidthProbe
    {
        TimePoint send_time;
        bool pending = false;
        qint64 bandwidth = 0;
    };

    QTimer* probe_timer_ = nullptr;
    TimePoint last_send_time_;
    BandwidthProbe tcp_probe_;
    BandwidthProbe udp_probe_;

    Q_DISABLE_COPY_MOVE(Client)
};

} // namespace host

Q_DECLARE_OPERATORS_FOR_FLAGS(host::Client::Features)

#endif // HOST_CLIENT_H
