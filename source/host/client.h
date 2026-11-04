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

#include <deque>
#include <list>

#include "base/logging.h"
#include "base/time_types.h"
#include "base/crypto/key_pair.h"
#include "base/net/tcp_channel.h"
#include "base/scoped_qpointer.h"
#include "host/bandwidth_controller.h"

namespace proto::peer {
enum SessionType : int;
class BandwidthProbeAck;
class ReceiveRate;
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

    void start(const QString& stun_host, quint16 stun_port);

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

    proto::desktop::Overflow::State overflowState(TimePoint now) const;

    enum class UdpState
    {
        DISCONNECTED,
        CONNECTED,
        READY
    };
    Q_ENUM(UdpState)

signals:
    void sig_started();
    void sig_finished();
    void sig_channelChanged();

protected:
    LOG_DECLARE_CONTEXT(Client);

    void send(quint8 channel_id, const QByteArray& buffer);

    virtual void onStart() = 0;
    virtual void onMessage(quint8 channel_id, const QByteArray& buffer) = 0;
    virtual void onBandwidthChanged(qint64 bandwidth) { /* Nothing */ };
    virtual void onTimer(TimePoint now);

    bool isFinished() const { return finished_emitted_; }
    qint64 publishedBandwidth() const { return published_bandwidth_; }

private slots:
    void onTcpErrorOccurred(TcpChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 tcp_channel_id, const QByteArray& buffer);
    void onSliceTimer();

private:
    void connectToUdp();
    void addAndStart(UdpAttempt* attempt);
    UdpAttempt* findAttempt(quint32 request_id);
    void eraseAttempt(quint32 request_id);
    void readUdpReply(const proto::peer::UdpReply& reply);
    void readUdpSwitch();
    void onAttemptConnected(quint32 request_id);
    void onAttemptError(quint32 request_id);
    void selectAttempt(UdpAttempt* attempt);
    void clearAttempts();
    void onUdpMessageReceived(quint8 channel_id, const QByteArray& buffer);
    void onUdpErrorOccurred();
    void sendDelayProbe();
    void onBandwidthProbeAck(const proto::peer::BandwidthProbeAck& ack);
    void onReceiveRate(const proto::peer::ReceiveRate& rate);
    void checkBandwidth();
    void publishBandwidth(qint64 bandwidth);
    void sendMetrics();
    void setUdpState(const Location& location, UdpState state);

    bool finished_emitted_ = false;
    TcpChannel* tcp_channel_ = nullptr;
    UdpState udp_state_ = UdpState::DISCONNECTED;
    ScopedQPointer<UdpChannel> udp_channel_;

    bool udp_switch_pending_ = false;
    std::deque<std::pair<quint8, QByteArray>> held_udp_messages_;

    std::list<ScopedQPointer<UdpAttempt>> attempts_;

    QString stun_host_;
    quint16 stun_port_ = 0;

    // Set when the peer's first message arrives; the bandwidth controller starts then (see
    // onTcpMessageReceived).
    bool peer_ready_ = false;

    quint32 next_train_id_ = 0;
    quint32 delay_probe_train_id_ = 0;
    BandwidthController bandwidth_controller_;
    QTimer* slice_timer_ = nullptr; // Sends the probes and the padding.
    qint64 published_bandwidth_ = 0; // Last estimate handed to onBandwidthChanged().

    Q_DISABLE_COPY_MOVE(Client)
};

#endif // HOST_CLIENT_H
