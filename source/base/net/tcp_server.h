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

#include <QQueue>
#include <QObject>

#include <asio/ip/address.hpp>

#include "base/net/tcp_channel.h"

namespace base {

class TcpChannel;

class TcpServer final : public QObject
{
    Q_OBJECT

public:
    explicit TcpServer(QObject* parent = nullptr);
    ~TcpServer();

    void start(quint16 port, const QString& iface = QString());

    bool hasPendingConnections();
    TcpChannel* nextPendingConnection();

    static bool isValidListenInterface(const QString& iface);

signals:
    void sig_newConnection();

private:
    void doAccept();

    asio::ip::tcp::acceptor acceptor_;
    int accept_error_count_ = 0;

    QQueue<TcpChannel*> pending_;

    Q_DISABLE_COPY(TcpServer)
};

} // namespace base

#endif // BASE_NET_TCP_SERVER_H
