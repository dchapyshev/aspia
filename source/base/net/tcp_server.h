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

#ifndef BASE_NET_TCP_SERVER_H
#define BASE_NET_TCP_SERVER_H

#include <QQueue>
#include <QObject>
#include <QStringList>

#include <asio/ip/tcp.hpp>

#include <memory>

#include "base/crypto/secure_byte_array.h"
#include "base/shared_pointer.h"
#include "base/peer/user_list.h"
#include "base/peer/server_authenticator.h"

class FloodGuard;
class TcpChannel;

class TcpServer final : public QObject
{
    Q_OBJECT

public:
    explicit TcpServer(QObject* parent = nullptr);
    ~TcpServer();

    void setUserList(SharedPointer<UserList> user_list);
    SharedPointer<UserList> userList() const { return user_list_; }

    void setPrivateKey(const SecureByteArray& private_key);
    const SecureByteArray& privateKey() const { return private_key_; }

    void setAnonymousAccess(
        ServerAuthenticator::AnonymousAccess anonymous_access, quint32 session_types);

    // Maximum number of handshakes that may be in progress simultaneously. New TCP connections
    // are accepted and immediately closed once the cap is reached. Defaults to 32 - tuned for a
    // single host. The router serves many peers and should raise this (e.g. to 128).
    void setMaxPendingConnections(int max_pending);

    // Maximum number of connection attempts accepted from a single peer address per minute.
    // Bursts of |max_per_minute| are absorbed before throttling kicks in. Defaults to 60
    // (~1/s with a burst of 60). Routers facing corporate NAT may need a much higher value.
    void setMaxConnectionsPerMinute(int max_per_minute);

    // Restricts the addresses allowed to connect. Entries are literal IPs or CIDR subnets; an empty
    // list allows any address. Rejected peers are dropped at accept time, before the handshake, so
    // unwanted traffic never consumes a pending slot.
    void setWhiteList(const QStringList& white_list);

    bool start(quint16 port, const QString& iface = QString());

    bool hasReadyConnections();
    TcpChannel* nextReadyConnection();

    static bool isValidListenInterface(const QString& iface);

signals:
    void sig_newConnection();
    void sig_errorOccurred(const QString& address, const QString& username);

private:
    void doAccept();
    void removePendingChannel(TcpChannel* channel);

    SharedPointer<bool> alive_guard_ { new bool(true) };
    asio::ip::tcp::acceptor acceptor_;
    int accept_error_count_ = 0;

    SharedPointer<UserList> user_list_;
    SecureByteArray private_key_;

    ServerAuthenticator::AnonymousAccess anonymous_access_ =
        ServerAuthenticator::AnonymousAccess::DISABLE;

    quint32 anonymous_session_types_ = 0;

    // Empty list means any address is allowed.
    QStringList white_list_;

    // Anti-flood gate: handles both per-address rate limiting and the global pending cap, plus
    // rate-limited rejection logging.
    std::unique_ptr<FloodGuard> flood_guard_;

    QQueue<TcpChannel*> pending_;
    QQueue<TcpChannel*> ready_;

    Q_DISABLE_COPY_MOVE(TcpServer)
};

#endif // BASE_NET_TCP_SERVER_H
