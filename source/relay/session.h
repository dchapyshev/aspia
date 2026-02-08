//
// SmartCafe Project
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

#ifndef RELAY_SESSION_H
#define RELAY_SESSION_H

#include <QObject>
#include <QByteArray>

#include <asio/ip/tcp.hpp>

#include "base/peer/host_id.h"

namespace base {
class Location;
} // namespace base

namespace relay {

class Session final : public QObject
{
    Q_OBJECT

public:
    Session(std::pair<asio::ip::tcp::socket, asio::ip::tcp::socket>&& sockets,
            const QByteArray& secret, QObject* parent = nullptr);
    ~Session();

    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    void start();
    void stop();
    void disconnect();

    quint64 sessionId() const { return session_id_; }
    const QString& clientAddress() const { return client_address_; }
    const QString& clientUserName() const { return client_user_name_; }
    const QString& hostAddress() const { return host_address_; }
    base::HostId hostId() const { return host_id_; }
    std::chrono::seconds idleTime(const TimePoint& current_time) const;
    std::chrono::seconds duration(const TimePoint& current_time) const;
    qint64 bytesTransferred() const { return bytes_transferred_; }

signals:
    void sig_sessionFinished(relay::Session* session);

private:
    static void doReadSome(Session* session, int source);
    void onErrorOccurred(const base::Location& location, const std::error_code& error_code);

    quint64 session_id_ = 0;
    QString client_address_;
    QString client_user_name_;
    QString host_address_;
    base::HostId host_id_ = base::kInvalidHostId;

    TimePoint start_time_;
    mutable TimePoint start_idle_time_;
    qint64 bytes_transferred_ = 0;

    static const int kNumberOfSides = 2;
    static const int kBufferSize = 8192;

    asio::ip::tcp::socket socket_[kNumberOfSides];
    std::array<quint8, kBufferSize> buffer_[kNumberOfSides];

    Q_DISABLE_COPY(Session)
};

} // namespace relay

#endif // RELAY_SESSION_H
