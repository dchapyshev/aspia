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

#ifndef RELAY_SESSIONS_WORKER_H
#define RELAY_SESSIONS_WORKER_H

#include "base/threading/thread.h"
#include "relay/session_manager.h"

namespace relay {

class SharedPool;

class SessionsWorker final : public QObject
{
    Q_OBJECT

public:
    SessionsWorker(const QString& listen_interface,
                   quint16 peer_port,
                   const std::chrono::minutes& peer_idle_timeout,
                   bool statistics_enabled,
                   const std::chrono::seconds& statistics_interval,
                   std::unique_ptr<SharedPool> shared_pool,
                   QObject* parent = nullptr);
    ~SessionsWorker() final;

public slots:
    void start();

signals:
    void sig_sessionStarted();
    void sig_sessionStatistics(const proto::RelayStat& relay_stat);
    void sig_sessionFinished();
    void sig_disconnectSession(quint64 session_id);

private slots:
    void onBeforeThreadRunning();
    void onAfterThreadRunning();

private:
    const QString listen_interface_;
    const quint16 peer_port_;
    const std::chrono::minutes peer_idle_timeout_;
    const bool statistics_enabled_;
    const std::chrono::seconds statistics_interval_;

    std::unique_ptr<SharedPool> shared_pool_;

    base::Thread thread_;
    std::unique_ptr<SessionManager> session_manager_;

    DISALLOW_COPY_AND_ASSIGN(SessionsWorker);
};

} // namespace relay

#endif // RELAY_SESSIONS_WORKER_H
