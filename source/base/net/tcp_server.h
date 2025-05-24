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

#ifndef BASE_NET_TCP_SERVER_H
#define BASE_NET_TCP_SERVER_H

#include <QObject>

#include "base/macros_magic.h"

#include <memory>
#include <queue>

namespace base {

class TcpChannel;

class TcpServer final : public QObject
{
    Q_OBJECT

public:
    explicit TcpServer(QObject* parent = nullptr);
    ~TcpServer();

    void start(const QString& listen_interface, quint16 port);
    void stop();
    bool hasPendingConnections();
    TcpChannel* nextPendingConnection();

    QString listenInterface() const;
    quint16 port() const;

    static bool isValidListenInterface(const QString& iface);

signals:
    void sig_newConnection();

private:
    void onNewConnection(std::unique_ptr<TcpChannel> channel);

    class Impl;
    std::shared_ptr<Impl> impl_;

    std::queue<std::unique_ptr<TcpChannel>> pending_;

    DISALLOW_COPY_AND_ASSIGN(TcpServer);
};

} // namespace base

#endif // BASE_NET_TCP_SERVER_H
