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

#include "proto/relay_peer.pb.h"
#include "proto/router_relay.pb.h"
#include "relay/pending_session.h"
#include "relay/session.h"
#include "relay/shared_pool.h"

#include <asio/high_resolution_timer.hpp>

namespace base {
class TaskRunner;
} // namespace base

namespace relay {

class SessionManager final
    : public PendingSession::Delegate,
      public Session::Delegate
{
public:
    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onSessionStarted() = 0;
        virtual void onSessionStatistics(const proto::RelayStat& relay_stat) = 0;
        virtual void onSessionFinished() = 0;
    };

    SessionManager(std::shared_ptr<base::TaskRunner> task_runner,
                   const asio::ip::address& address,
                   quint16 port,
                   const std::chrono::minutes& idle_timeout,
                   bool statistics_enabled,
                   const std::chrono::seconds& statistics_interval);
    ~SessionManager() final;

    void start(std::unique_ptr<SharedPool> shared_pool, Delegate* delegate);
    void disconnectSession(uint64_t session_id);

protected:
    // PendingSession::Delegate implementation.
    void onPendingSessionReady(
        PendingSession* session, const proto::PeerToRelay& message) final;
    void onPendingSessionFailed(PendingSession* session) final;

    // Session::Delegate implementation.
    void onSessionFinished(Session* session) final;

private:
    static void doAccept(SessionManager* self);
    static void doIdleTimeout(SessionManager* self, const std::error_code& error_code);
    void doIdleTimeoutImpl(const std::error_code& error_code);
    static void doStatTimeout(SessionManager* self, const std::error_code& error_code);
    void doStatTimeoutImpl(const std::error_code& error_code);
    void collectAndSendStatistics();

    void removePendingSession(PendingSession* sessions);
    void removeSession(Session* session);

    std::shared_ptr<base::TaskRunner> task_runner_;

    asio::ip::tcp::acceptor acceptor_;
    std::vector<std::unique_ptr<PendingSession>> pending_sessions_;
    std::vector<std::unique_ptr<Session>> active_sessions_;

    const asio::ip::address address_;
    const quint16 port_;

    const std::chrono::minutes idle_timeout_;
    asio::steady_timer idle_timer_;

    asio::steady_timer stat_timer_;

    std::unique_ptr<SharedPool> shared_pool_;
    Delegate* delegate_ = nullptr;

    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    TimePoint start_time_;

    bool statistics_enabled_ = false;
    std::chrono::seconds statistics_interval_;

    DISALLOW_COPY_AND_ASSIGN(SessionManager);
};

} // namespace relay

#endif // RELAY_SESSION_MANAGER_H
