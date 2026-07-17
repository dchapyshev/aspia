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
#include "client/client.h"
#include "client/session_state.h"

class QTimer;
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
    // Starts a direct or relay connection according to the session state. Runs in the worker
    // thread; invoke it through a queued connection (as all other slots).
    void onStartConnection(std::shared_ptr<SessionState> session_state);

    // Called when the session is set up and ready to receive incoming messages. Unpauses the
    // channel and starts the receive-rate reporting.
    void onSessionReady();

    // Sends outgoing session message.
    void onSendMessage(quint8 channel_id, const QByteArray& buffer);

    // Emits sig_metrics with the current network metrics.
    void onMetricsRequest();

signals:
    void sig_statusChanged(Client::Status status, const QVariant& data = QVariant());
    // Connection established, authentication passed and versions are compatible. The session can
    // be started; call onSessionReady() to begin receiving messages.
    void sig_connected();
    // Incoming session message (the peer control channel is handled internally).
    void sig_messageReceived(quint8 channel_id, const QByteArray& buffer);
    void sig_metrics(const NetworkWorker::Metrics& metrics);

protected:
    // Worker implementation.
    void onStart() final;
    void onStop() final;

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
    void onReceiveRateReport();

private:
    void startConnection();
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
    qint64 totalTcpRx() const;
    qint64 totalUdpRx() const;

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

    // Receive-rate reporting (see onReceiveRateReport).
    QTimer* receive_rate_timer_ = nullptr;
    QElapsedTimer receive_rate_interval_;
    qint64 receive_rate_last_total_ = 0;

    Q_DISABLE_COPY_MOVE(NetworkWorker)
};

Q_DECLARE_METATYPE(NetworkWorker::Metrics)

#endif // CLIENT_WORKERS_NETWORK_WORKER_H
