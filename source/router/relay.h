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

#ifndef ROUTER_RELAY_H
#define ROUTER_RELAY_H

#include <QObject>
#include <QVersionNumber>

#include "base/logging.h"
#include "base/net/tcp_channel.h"
#include "base/serialization.h"
#include "proto/router_relay.h"

namespace proto::router {
class PeerRequest;
} // namespace proto::router

class Relay final : public QObject
{
    Q_OBJECT

public:
    Relay(TcpChannel* channel, QObject* parent);
    ~Relay() final;

    void start();

    QVersionNumber version() const;
    const std::string& osName() const;
    const std::string& computerName() const;
    const std::string& architecture() const;

    qint64 sessionId() const { return session_id_; }
    const std::string& address() const { return tcp_channel_->peerAddress(); }
    time_t startTime() const { return start_time_; }

    void sendMessage(quint8 channel_id, const QByteArray& message);

    const std::optional<proto::router::RelayStatistics>& statistics() const { return statistics_; }
    void sendKeyUsed(quint32 key_id);
    void disconnectPeerSession(const proto::router::PeerRequest& request);

signals:
    void sig_started(qint64 session_id);
    void sig_finished(qint64 session_id);

private slots:
    void onTcpErrorOccurred(TcpChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer);

private:
    void readKeyPool(const proto::router::RelayKeyPool& key_pool);

    const qint64 session_id_;
    time_t start_time_ = 0;

    TcpChannel* tcp_channel_ = nullptr;

    std::optional<proto::router::RelayStatistics> statistics_;

    Parser<proto::router::RelayToRouter> incoming_message_;
    Serializer<proto::router::RouterToRelay> outgoing_message_;

    LOG_DECLARE_CONTEXT(Relay);
    Q_DISABLE_COPY_MOVE(Relay)
};

#endif // ROUTER_RELAY_H
