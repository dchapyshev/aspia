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

#ifndef CLIENT_WORKERS_NETWORK_WORKER_H
#define CLIENT_WORKERS_NETWORK_WORKER_H

#include <QElapsedTimer>
#include <QVariant>

#include <list>
#include <memory>
#include <optional>

#include "base/net/tcp_channel.h"
#include "base/scoped_qpointer.h"
#include "base/threading/worker.h"
#include "client/session_state.h"

class RelayPeer;
class UdpAttempt;
class UdpChannel;

namespace proto::peer {
class BandwidthProbe;
class DirectUdpRequest;
class StunUdpRequest;
class GatewayUdpRequest;
} // namespace proto::peer

enum class UdpMethod;

class NetworkWorker final : public Worker
{
    Q_OBJECT

public:
    NetworkWorker();
    ~NetworkWorker() final;

    enum class Status
    {
        STARTED,
        HOST_CONNECTING,
        HOST_CONNECTED,
        HOST_DISCONNECTED,
        WAIT_FOR_HOST,
        VERSION_MISMATCH,
        LEGACY_HOST, // Remove this after support for versions below 3.0.0 ends.
        RELAY_ERROR
    };
    Q_ENUM(Status)

    struct Metrics
    {
        qint64 total_tcp_rx = 0;
        qint64 total_tcp_tx = 0;
        int speed_tcp_rx = 0;
        int speed_tcp_tx = 0;
        qint64 total_udp_rx = 0;
        qint64 total_udp_tx = 0;
        int speed_udp_rx = 0;
        int speed_udp_tx = 0;
        UdpMethod udp_method {};
    };

public slots:
    // Starts a direct or relay connection according to the session state.
    void onStartConnection(std::shared_ptr<SessionState> session_state);

    // Called when the session is set up and ready to receive incoming messages.
    void onSessionReady();

    // Sends outgoing session message.
    void onSendMessage(quint8 channel_id, const QByteArray& buffer);

signals:
    void sig_statusChanged(NetworkWorker::Status status, const QVariant& data = QVariant());
    void sig_channel_0(const QByteArray& buffer);
    void sig_channel_1(const QByteArray& buffer);
    void sig_channel_2(const QByteArray& buffer);
    void sig_channel_3(const QByteArray& buffer);
    void sig_channel_4(const QByteArray& buffer);
    void sig_channel_5(const QByteArray& buffer);
    void sig_channel_6(const QByteArray& buffer);
    void sig_channel_7(const QByteArray& buffer);
    void sig_channel_8(const QByteArray& buffer);
    void sig_channel_9(const QByteArray& buffer);
    void sig_channel_10(const QByteArray& buffer);
    void sig_channel_11(const QByteArray& buffer);
    void sig_metrics(const NetworkWorker::Metrics& metrics);

protected:
    // Worker implementation.
    void onStart() final;
    void onStop() final;
    void onTimer(const TimePoint& now) final;

private slots:
    void onTcpConnected();
    void onTcpErrorOccurred(TcpChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer);
    void onUdpMessageReceived(quint8 channel_id, const QByteArray& buffer);
    void onUdpErrorOccurred();
    void onRelayConnectionReady();
    void onRelayConnectionError(std::optional<TcpChannel::ErrorCode> error_code);
    void onAttemptConnected(quint32 request_id);
    void onAttemptError(quint32 request_id);

private:
    void startConnection();
    void routeMessage(quint8 channel_id, const QByteArray& buffer);
    void tcpChannelReady();
    void readBandwidthProbe(const proto::peer::BandwidthProbe& probe, bool via_udp);
    void readDirectUdpRequest(const proto::peer::DirectUdpRequest& request);
    void readStunUdpRequest(const proto::peer::StunUdpRequest& request);
    void readGatewayUdpRequest(const proto::peer::GatewayUdpRequest& request);
    void addAndStart(UdpAttempt* attempt);
    UdpAttempt* findAttempt(quint32 request_id);
    void eraseAttempt(quint32 request_id);
    void clearAttempts();
    void selectAttempt(UdpAttempt* attempt);

    bool is_legacy_mode_ = false;
    ScopedQPointer<TcpChannel> tcp_channel_;
    ScopedQPointer<UdpChannel> udp_channel_;
    ScopedQPointer<RelayPeer> relay_peer_;

    std::list<ScopedQPointer<UdpAttempt>> attempts_;
    std::shared_ptr<SessionState> session_state_;

    bool udp_ready_ = false;
    const quint32 udp_methods_;

    // Arrival state of the probe train currently being received (see readBandwidthProbe). A train
    // that never completes (UDP loss) is simply superseded by the next one.
    struct ProbeTrain
    {
        quint32 id = 0;
        bool active = false;
        QElapsedTimer first_arrival; // Started when the first probe of the train arrives.
        quint64 bytes = 0;           // Payload bytes received after the first probe.
    };
    ProbeTrain probe_train_;

    // Set once the session is ready; gates the periodic work in onTimer().
    bool session_ready_ = false;

    // Receive-rate reporting (see onTimer).
    QElapsedTimer receive_rate_interval_;
    qint64 receive_rate_last_total_ = 0;

    Q_DISABLE_COPY_MOVE(NetworkWorker)
};

Q_DECLARE_METATYPE(NetworkWorker::Metrics)
Q_DECLARE_METATYPE(NetworkWorker::Status)

#endif // CLIENT_WORKERS_NETWORK_WORKER_H
