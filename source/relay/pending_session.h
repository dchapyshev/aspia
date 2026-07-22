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

#ifndef RELAY_PENDING_SESSION_H
#define RELAY_PENDING_SESSION_H

#include <QObject>
#include <QByteArray>

#include <asio/ip/tcp.hpp>

#include "base/logging.h"
#include "base/shared_pointer.h"
#include "base/time_types.h"

class Location;

namespace proto::relay {
class PeerToRelay;
} // namespace proto::relay

class PendingSession final : public QObject
{
    Q_OBJECT

public:
    PendingSession(asio::ip::tcp::socket&& socket, QObject* parent = nullptr);
    ~PendingSession();

    // Starts a session. This starts the timer. If the peer does not send authentication data or if
    // the opposite side peer is not found during this time, then method onPendingSessionFailed()
    // will be called.
    void start();

    // Sets session credentials.
    void setIdentify(quint32 key_id, const QByteArray& secret);

    // Returns true if the other session is a pair and false otherwise.
    bool isPeerFor(const PendingSession& other) const;

    // Releases a socket from a class.
    asio::ip::tcp::socket takeSocket();

    const QString& address() const;
    Seconds duration(TimePoint now) const;
    quint32 keyId() const;

    // Returns true if the session has passed its deadline (handshake or total-lifetime budget). The
    // session has no internal timer; the owner drives the check from its clock.
    bool isExpired(TimePoint now) const;

signals:
    // Called when received authentication data from a peer.
    void sig_ready(const proto::relay::PeerToRelay& message);

    // Called when an error has occurred.
    void sig_failed();

private:
    static void doReadMessage(PendingSession* pending_session);
    void onErrorOccurred(const Location& location, const std::error_code& error_code);
    void onMessage();

    struct IoState
    {
        bool alive = true;
        quint32 buffer_size = 0;
        QByteArray buffer;
    };

    SharedPointer<IoState> io_ { new IoState() };

    QString address_;
    TimePoint start_time_;

    // Absolute deadline for the current phase: start_time_ + handshake timeout before the handshake,
    // extended to start_time_ + total-lifetime budget once it arrives.
    TimePoint deadline_;

    asio::ip::tcp::socket socket_;

    QByteArray secret_;
    quint32 key_id_ = static_cast<quint32>(-1);

    LOG_DECLARE_CONTEXT(PendingSession);
    Q_DISABLE_COPY_MOVE(PendingSession)
};

#endif // RELAY_PENDING_SESSION_H
