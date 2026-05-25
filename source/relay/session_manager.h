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

#include "base/crypto/secure_byte_array.h"
#include "base/shared_pointer.h"
#include "relay/session_key.h"

#include <memory>

class FloodGuard;
class PendingSession;
class QTimer;
class Session;

namespace proto::relay {
class PeerToRelay;
} // namespace proto::relay

namespace proto::router {
class RelayStatistics;
} // namespace proto::router

class SessionManager final : public QObject
{
    Q_OBJECT

public:
    explicit SessionManager(QObject* parent = nullptr);
    ~SessionManager() final;

    using Key = std::pair<SecureByteArray, QByteArray>;

    bool start();
    quint32 addKey(SessionKey&& session_key);
    bool removeKey(quint32 key_id);
    void setKeyExpired(quint32 key_id);
    std::optional<Key> keyFromPool(quint32 key_id, const std::string& peer_public_key) const;
    void clearKeys();

public slots:
    void onDisconnectSession(qint64 session_id);

signals:
    void sig_started();
    void sig_statistics(const proto::router::RelayStatistics& statistics);
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
    quint32 key_counter_ = 0;

    SharedPointer<bool> alive_guard_ { new bool(true) };
    asio::ip::tcp::acceptor acceptor_;
    QList<PendingSession*> pending_sessions_;
    QList<Session*> active_sessions_;

    std::chrono::minutes idle_timeout_;
    QTimer* idle_timer_ = nullptr;
    QTimer* stat_timer_ = nullptr;

    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    TimePoint start_time_;

    // Anti-flood gate: per-address rate limit + global pending cap + rate-limited logging.
    std::unique_ptr<FloodGuard> flood_guard_;

    Q_DISABLE_COPY_MOVE(SessionManager)
};

#endif // RELAY_SESSION_MANAGER_H
