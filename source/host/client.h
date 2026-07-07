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
#include <list>

#include "base/logging.h"
#include "base/crypto/key_pair.h"
#include "base/net/tcp_channel.h"
#include "base/scoped_qpointer.h"

namespace proto::peer {
enum SessionType : int;
class UdpReply;
} // namespace proto::peer

class Location;
class QTimer;
class UdpAttempt;
class UdpChannel;

class Client : public QObject
{
    Q_OBJECT

public:
    Client(TcpChannel* tcp_channel, QObject* parent);
    virtual ~Client();

    enum Feature
    {
        FEATURE_NONE      = 0,
        FEATURE_UDP       = 1,
        FEATURE_BANDWIDTH = 2
    };
    Q_DECLARE_FLAGS(Features, Feature)

    void start(const QString& stun_host, quint16 stun_port);
    void setFeature(Feature feature, bool enable);

    // Tears the client down, emitting sig_finished() exactly once. Idempotent: safe to call from any
    // path and any number of times. This is the ONLY place sig_finished() is emitted - never emit that
    // signal directly, or a duplicate can crash the service (see finish() in client.cc).
    void finish();

    quint32 clientId() const;
    proto::peer::SessionType sessionType() const;
    QVersionNumber version() const;
    QString address() const;
    QString osName() const;
    QString computerName() const;
    QString displayName() const;
    QString architecture() const;
    QString userName() const;
    qint64 pendingBytes() const;
    qint64 bandwidth() const;

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
    void sig_channelChanged();

protected:
    LOG_DECLARE_CONTEXT(Client);

    void send(quint8 channel_id, const QByteArray& buffer, bool reliable = true);

    virtual void onStart() = 0;
    virtual void onMessage(quint8 channel_id, const QByteArray& buffer) = 0;
    virtual void onBandwidthChanged(qint64 bandwidth) { /* Nothing */ };

    bool isFinished() const { return finished_emitted_; }

private slots:
    void onTcpErrorOccurred(TcpChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 tcp_channel_id, const QByteArray& buffer);

private:
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Milliseconds = std::chrono::milliseconds;

    void connectToUdp();
    void addAndStart(UdpAttempt* attempt);
    UdpAttempt* findAttempt(quint32 request_id);
    void eraseAttempt(quint32 request_id);
    void readUdpReply(const proto::peer::UdpReply& reply);
    void onAttemptConnected(quint32 request_id, qint64 bandwidth);
    void onAttemptError(quint32 request_id);
    void selectAttempt(UdpAttempt* attempt, qint64 bandwidth);
    void clearAttempts();
    void onUdpMessageReceived(quint8 channel_id, const QByteArray& buffer);
    void onUdpErrorOccurred();
    void startBandwidthProbing();
    void sendTcpBandwidthProbe(const TimePoint& time);
    void sendUdpBandwidthProbe(const TimePoint& time);
    void onTcpBandwidthProbeAck();
    void onUdpBandwidthProbeAck();
    void checkBandwidth();
    void setUdpState(const Location& location, UdpState state);

    bool finished_emitted_ = false;
    Features features_ = FEATURE_NONE;
    TcpChannel* tcp_channel_ = nullptr;
    UdpState udp_state_ = UdpState::DISCONNECTED;
    ScopedQPointer<UdpChannel> udp_channel_;

    std::list<ScopedQPointer<UdpAttempt>> attempts_;

    QString stun_host_;
    quint16 stun_port_ = 0;

    struct BandwidthProbe
    {
        TimePoint send_time;
        bool pending = false;
        qint64 bandwidth = 0;
        qint64 size = 0; // Payload size of the in-flight probe (for the bandwidth calculation).
    };

    QTimer* probe_timer_ = nullptr;
    TimePoint last_send_time_;
    BandwidthProbe tcp_probe_;
    BandwidthProbe udp_probe_;

    Q_DISABLE_COPY_MOVE(Client)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Client::Features)

#endif // HOST_CLIENT_H
