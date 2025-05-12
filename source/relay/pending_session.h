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

#ifndef RELAY_PENDING_SESSION_H
#define RELAY_PENDING_SESSION_H

#include "base/macros_magic.h"
#include "base/peer/host_id.h"
#include "proto/relay_peer.pb.h"

#include <asio/ip/tcp.hpp>

#include <QTimer>
#include <QByteArray>

namespace base {
class Location;
} // namespace base

namespace relay {

class PendingSession final : public QObject
{
    Q_OBJECT

public:
    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        // Called when received authentication data from a peer.
        virtual void onPendingSessionReady(
            PendingSession* session, const proto::PeerToRelay& message) = 0;

        // Called when an error has occurred.
        virtual void onPendingSessionFailed(PendingSession* session) = 0;
    };

    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    PendingSession(asio::ip::tcp::socket&& socket,
                   Delegate* delegate,
                   QObject* parent = nullptr);
    ~PendingSession();

    // Starts a session. This starts the timer. If the peer does not send authentication data or if
    // the opposite side peer is not found during this time, then method onPendingSessionFailed()
    // will be called.
    void start();

    // Stops a session. No notifications will not come after calling this method.
    void stop();

    // Sets session credentials.
    void setIdentify(quint32 key_id, const QByteArray& secret);

    // Returns true if the other session is a pair and false otherwise.
    bool isPeerFor(const PendingSession& other) const;

    // Releases a socket from a class.
    asio::ip::tcp::socket takeSocket();

    const std::string& address() const;
    std::chrono::seconds duration(const TimePoint& now) const;
    quint32 keyId() const;

private:
    static void doReadMessage(PendingSession* pending_session);
    void onErrorOccurred(const base::Location& location, const std::error_code& error_code);
    void onMessage();

    Delegate* delegate_;

    std::string address_;
    TimePoint start_time_;
    QTimer timer_;
    asio::ip::tcp::socket socket_;

    quint32 buffer_size_ = 0;
    QByteArray buffer_;

    QByteArray secret_;
    quint32 key_id_ = static_cast<quint32>(-1);

    DISALLOW_COPY_AND_ASSIGN(PendingSession);
};

} // namespace relay

#endif // RELAY_PENDING_SESSION_H
