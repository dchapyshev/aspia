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

#ifndef RELAY_SERVICE_H
#define RELAY_SERVICE_H

#include <QTimer>

#include "base/serialization.h"
#include "base/service.h"
#include "base/net/tcp_channel.h"
#include "proto/router_relay.h"

namespace relay {

class SessionManager;

class Service final : public base::Service
{
    Q_OBJECT

public:
    explicit Service(QObject* parent = nullptr);
    ~Service() final;

    static const char kFileName[];
    static const char kName[];
    static const char kDisplayName[];
    static const char kDescription[];

protected:
    // base::Service implementation.
    void onStart() final;
    void onStop() final;

private slots:
    void onPoolKeyExpired(quint32 key_id);
    void onTcpConnected();
    void onTcpErrorOccurred(base::TcpChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer);
    void onSessionStarted();
    void onSessionStatistics(const proto::router::RelayStat& relay_stat);
    void onSessionFinished();

private:
    bool start();
    void connectToRouter();
    void delayedConnectToRouter();
    void sendKeyPool(quint32 key_count);

    // Router settings.
    QString router_address_;
    quint16 router_port_ = 0;
    QByteArray router_public_key_;

    // Peers settings.
    QString peer_address_;
    quint16 peer_port_ = 0;
    quint32 max_peer_count_ = 0;

    QTimer* reconnect_timer_ = nullptr;
    base::TcpChannel* tcp_channel_ = nullptr;
    SessionManager* session_manager_ = nullptr;

    base::Parser<proto::router::RouterToRelay> incoming_message_;
    base::Serializer<proto::router::RelayToRouter> outgoing_message_;

    int session_count_ = 0;

    Q_DISABLE_COPY_MOVE(Service)
};

} // namespace relay

#endif // RELAY_SERVICE_H
