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

#ifndef RELAY_SESSION_MANAGER_H
#define RELAY_SESSION_MANAGER_H

#include <QList>
#include <QTimer>

#include "proto/relay_peer.pb.h"
#include "proto/router_relay.pb.h"
#include "relay/pending_session.h"
#include "relay/session.h"
#include "relay/shared_pool.h"

#include <asio/steady_timer.hpp>

namespace relay {

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

    void start(std::unique_ptr<SharedPool> shared_pool);

public slots:
    void disconnectSession(quint64 session_id);

signals:
    void sig_sessionStarted();
    void sig_sessionStatistics(const proto::RelayStat& relay_stat);
    void sig_sessionFinished();

private slots:
    void onPendingSessionReady(relay::PendingSession* session, const proto::PeerToRelay& message);
    void onPendingSessionFailed(relay::PendingSession* session);
    void onSessionFinished(relay::Session* session);

private:
    static void doAccept(SessionManager* self);
    void onIdleTimeout();
    void onStatTimeout();

    void removePendingSession(PendingSession* sessions);
    void removeSession(Session* session);

    asio::ip::tcp::acceptor acceptor_;
    QList<PendingSession*> pending_sessions_;
    QList<Session*> active_sessions_;

    const asio::ip::address address_;
    const quint16 port_;

    const std::chrono::minutes idle_timeout_;
    QTimer* idle_timer_ = nullptr;

    QTimer* stat_timer_ = nullptr;

    std::unique_ptr<SharedPool> shared_pool_;

    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    TimePoint start_time_;

    bool statistics_enabled_ = false;
    std::chrono::seconds statistics_interval_;

    DISALLOW_COPY_AND_ASSIGN(SessionManager);
};

} // namespace relay

#endif // RELAY_SESSION_MANAGER_H
