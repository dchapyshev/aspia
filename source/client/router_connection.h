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

#ifndef CLIENT_ROUTER_CONNECTION_H
#define CLIENT_ROUTER_CONNECTION_H

#include <QObject>

#include "base/net/tcp_channel.h"
#include "client/config.h"

namespace client {

class RouterConnection final : public QObject
{
    Q_OBJECT

public:
    enum class Status
    {
        OFFLINE,
        CONNECTING,
        ONLINE
    };
    Q_ENUM(Status)

    explicit RouterConnection(const RouterConfig& config, QObject* parent = nullptr);
    ~RouterConnection() final;

    Status status() const { return status_; }
    const RouterConfig& config() const;
    const QUuid& uuid() const;

public slots:
    void onConnectToRouter();
    void onDisconnectFromRouter();
    void onUpdateConfig(const client::RouterConfig& config);

signals:
    void sig_statusChanged(const QUuid& uuid, client::RouterConnection::Status status);

private slots:
    void onTcpReady();
    void onTcpErrorOccurred(base::TcpChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer);

private:
    void setStatus(Status status);

    RouterConfig config_;
    base::TcpChannel* tcp_channel_ = nullptr;
    Status status_ = Status::OFFLINE;

    Q_DISABLE_COPY_MOVE(RouterConnection)
};

} // namespace client

#endif // CLIENT_ROUTER_CONNECTION_H
