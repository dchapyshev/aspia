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

#include "base/shared_pointer.h"
#include "base/net/tcp_channel.h"
#include "base/peer/user_list_base.h"
#include "base/peer/server_authenticator.h"

namespace base {

class TcpChannel;

class TcpServer final : public QObject
{
    Q_OBJECT

public:
    explicit TcpServer(QObject* parent = nullptr);
    ~TcpServer();

    void setUserList(SharedPointer<UserListBase> user_list);
    SharedPointer<UserListBase> userList() const { return user_list_; }

    void setPrivateKey(const QByteArray& private_key);
    QByteArray privateKey() const { return private_key_; }

    void setAnonymousAccess(
        ServerAuthenticator::AnonymousAccess anonymous_access, quint32 session_types);

    void start(quint16 port, const QString& iface = QString());

    bool hasReadyConnections();
    TcpChannel* nextReadyConnection();

    static bool isValidListenInterface(const QString& iface);

signals:
    void sig_newConnection();

private:
    void doAccept();
    void removePendingChannel(TcpChannel* channel);

    asio::ip::tcp::acceptor acceptor_;
    int accept_error_count_ = 0;

    SharedPointer<UserListBase> user_list_;
    QByteArray private_key_;

    ServerAuthenticator::AnonymousAccess anonymous_access_ =
        ServerAuthenticator::AnonymousAccess::DISABLE;

    quint32 anonymous_session_types_ = 0;

    QQueue<TcpChannel*> pending_;
    QQueue<TcpChannel*> ready_;

    Q_DISABLE_COPY(TcpServer)
};

} // namespace base

#endif // BASE_NET_TCP_SERVER_H
