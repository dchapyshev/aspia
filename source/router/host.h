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

#ifndef ROUTER_HOST_H
#define ROUTER_HOST_H

#include <QByteArray>
#include <QObject>
#include <QVersionNumber>

#include "base/logging.h"
#include "base/time_types.h"
#include "base/net/tcp_channel.h"
#include "base/peer/host_id.h"

class Database;

namespace proto::router {
class ConnectionOffer;
class HostIdRequest;
class HostTelemetry;
} // namespace proto::router

class Host final : public QObject
{
    Q_OBJECT

public:
    // |database| is the connection of the thread the host object runs in; it outlives every host
    // object of that thread. Passing it instead of reaching for the per-thread singleton is what
    // lets a host object be driven against a temporary database in a test.
    Host(Database& database, TcpChannel* channel, QObject* parent);
    ~Host() final;

    void start();
    void stop();

    QVersionNumber version() const;
    const std::string& osName() const;
    const std::string& computerName() const;
    const std::string& architecture() const;

    qint64 sessionId() const { return session_id_; }
    const std::string& address() const { return tcp_channel_->peerAddress(); }
    TimePoint startTime() const { return start_time_; }

    HostId hostId() const { return host_id_; }
    bool isRemoveCommandSent() const { return remove_command_sent_; }
    const QByteArray& keyHash() const { return key_hash_; }
    const QByteArray& hardwareId() const { return hw_id_; }
    bool isPortable() const { return portable_; }

    void sendConnectionOffer(const proto::router::ConnectionOffer& offer);
    // Sends the "remove" host command and marks the connection so that on disconnect the
    // hosts_remove row for this host_id is finalized. TCP delivers the command reliably; the
    // host's disconnect is treated as a proof of receipt.
    void sendRemoveCommand();
    // Sends the "update" host command, asking the host to check for updates immediately.
    void sendUpdateCommand();

signals:
    void sig_started(qint64 session_id);
    void sig_finished(qint64 session_id);
    void sig_notifyChanged(quint32 flags);
    void sig_hostIdAssigned(HostId host_id);

private slots:
    void onTcpErrorOccurred(TcpChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer);

private:
    void sendMessage(quint8 channel_id, const QByteArray& message);
    void readHostIdRequest(const proto::router::HostIdRequest& host_id_request);
    void readHostTelemetry(const proto::router::HostTelemetry& host_telemetry, TimePoint now);

    Database& database_;
    const qint64 session_id_;
    TimePoint start_time_;
    TcpChannel* tcp_channel_ = nullptr;

    HostId host_id_ = kInvalidHostId;
    QByteArray key_hash_;
    QByteArray hw_id_;
    bool portable_ = false;
    std::string telemetry_;
    TimePoint next_telemetry_time_ = TimePoint::min();
    int id_request_count_ = 0;
    bool remove_command_sent_ = false;

    friend class HostTestPeer;
    LOG_DECLARE_CONTEXT(Host);
    Q_DISABLE_COPY_MOVE(Host)
};

#endif // ROUTER_HOST_H
