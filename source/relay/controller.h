//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef RELAY_CONTROLLER_H
#define RELAY_CONTROLLER_H

#include "base/net/tcp_channel.h"
#include "proto/router_relay.pb.h"
#include "relay/sessions_worker.h"
#include "relay/key_factory.h"

#include <QPointer>
#include <QTimer>

namespace base {
class ClientAuthenticator;
} // namespace base

namespace relay {

class Controller final: public QObject
{
    Q_OBJECT

public:
    explicit Controller(QObject* parent = nullptr);
    ~Controller() final;

    bool start();

private slots:
    void onPoolKeyExpired(quint32 key_id);
    void onTcpConnected();
    void onTcpDisconnected(base::NetworkChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer);
    void onSessionStarted();
    void onSessionStatistics(const proto::RelayStat& relay_stat);
    void onSessionFinished();

private:
    void connectToRouter();
    void delayedConnectToRouter();
    void sendKeyPool(quint32 key_count);

    // Router settings.
    QString router_address_;
    quint16 router_port_ = 0;
    QByteArray router_public_key_;

    // Peers settings.
    QString listen_interface_;
    QString peer_address_;
    quint16 peer_port_ = 0;
    std::chrono::minutes peer_idle_timeout_;
    quint32 max_peer_count_ = 0;
    bool statistics_enabled_ = false;
    std::chrono::seconds statistics_interval_;

    QPointer<QTimer> reconnect_timer_;
    base::TcpChannel* channel_ = nullptr;
    QPointer<base::ClientAuthenticator> authenticator_;
    KeyFactory* key_factory_ = nullptr;
    SessionsWorker* sessions_worker_ = nullptr;

    proto::RouterToRelay incoming_message_;
    proto::RelayToRouter outgoing_message_;

    int session_count_ = 0;

    DISALLOW_COPY_AND_ASSIGN(Controller);
};

} // namespace relay

#endif // RELAY_CONTROLLER_H
