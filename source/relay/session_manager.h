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

#ifndef RELAY_SESSION_MANAGER_H
#define RELAY_SESSION_MANAGER_H

#include <QObject>

#include <asio/ip/tcp.hpp>
#include <asio/steady_timer.hpp>

#include "proto/relay_peer.h"
#include "proto/router_relay.h"
#include "relay/session_key.h"

class QTimer;

namespace relay {

class PendingSession;
class Session;

class SessionManager final : public QObject
{
    Q_OBJECT

public:
    SessionManager(const asio::ip::address& address,
                   quint16 port,
                   const std::chrono::minutes& idle_timeout,
                   bool statistics_enabled,
                   const std::chrono::seconds& statistics_interval,
                   QObject* parent = nullptr);
    ~SessionManager() final;

    using Key = std::pair<QByteArray, QByteArray>;

    void start();
    quint32 addKey(SessionKey&& session_key);
    bool removeKey(quint32 key_id);
    void setKeyExpired(quint32 key_id);
    std::optional<Key> keyFromPool(quint32 key_id, const std::string& peer_public_key) const;
    void clearKeys();

public slots:
    void onDisconnectSession(quint64 session_id);

signals:
    void sig_started();
    void sig_statistics(const proto::router::RelayStat& relay_stat);
    void sig_keyExpired(quint32 key_id);
    void sig_finished();

private slots:
    void onPendingSessionReady(const proto::relay::PeerToRelay& message);
    void onPendingSessionFailed();
    void onSessionFinished();

private:
    static void doAccept(SessionManager* self);
    void onIdleTimeout();
    void onStatTimeout();

    void removePendingSession(PendingSession* sessions);
    void removeSession(Session* session);

    std::unordered_map<quint32, SessionKey> key_pool_;
    quint32 current_key_id_ = 0;

    asio::ip::tcp::acceptor acceptor_;
    QList<PendingSession*> pending_sessions_;
    QList<Session*> active_sessions_;

    const asio::ip::address address_;
    const quint16 port_;

    const std::chrono::minutes idle_timeout_;
    QTimer* idle_timer_ = nullptr;
    QTimer* stat_timer_ = nullptr;

    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    TimePoint start_time_;

    bool statistics_enabled_ = false;
    std::chrono::seconds statistics_interval_;

    Q_DISABLE_COPY_MOVE(SessionManager)
};

} // namespace relay

#endif // RELAY_SESSION_MANAGER_H
