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

#ifndef ROUTER_SESSION_H
#define ROUTER_SESSION_H

#include <QObject>
#include <QVersionNumber>

#include "base/shared_pointer.h"
#include "base/net/tcp_channel.h"
#include "proto/router.h"
#include "router/key_pool.h"

namespace router {

class Database;
class DatabaseFactory;
class SessionManager;

class Session : public QObject
{
    Q_OBJECT

public:
    Session(base::TcpChannel* channel, QObject* parent);
    virtual ~Session() override;

    using SessionId = qint64;

    void setRelayKeyPool(base::SharedPointer<KeyPool> relay_key_pool);
    void setDatabaseFactory(base::SharedPointer<DatabaseFactory> database_factory);

    void start();

    QVersionNumber version() const;
    QString osName() const;
    QString computerName() const;
    QString architecture() const;
    QString userName() const;
    proto::router::SessionType sessionType() const;

    SessionId sessionId() const { return session_id_; }
    const QString& address() const { return address_; }
    time_t startTime() const { return start_time_; }
    std::chrono::seconds duration() const;

signals:
    void sig_sessionFinished(SessionId session_id);

protected:
    void sendMessage(const QByteArray& message);
    std::unique_ptr<Database> openDatabase() const;

    virtual void onSessionReady() = 0;
    virtual void onSessionMessageReceived(quint8 channel_id, const QByteArray& buffer) = 0;
    virtual void onSessionMessageWritten(quint8 channel_id, size_t pending) = 0;

    KeyPool& relayKeyPool() { return *relay_key_pool_; }
    const KeyPool& relayKeyPool() const { return *relay_key_pool_; }

    SessionManager* sessionManager() const;
    QList<Session*> sessions() const;
    Session* sessionById(SessionId session_id) const;

private slots:
    void onTcpErrorOccurred(base::TcpChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer);
    void onTcpMessageWritten(quint8 channel_id, size_t pending);

private:
    const SessionId session_id_;
    time_t start_time_ = 0;

    base::TcpChannel* tcp_channel_ = nullptr;

    base::SharedPointer<DatabaseFactory> database_factory_;
    base::SharedPointer<KeyPool> relay_key_pool_;
    QString address_;
};

} // namespace router

#endif // ROUTER_SESSION_H
