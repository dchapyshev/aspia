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

#ifndef CLIENT_ROUTER_H
#define CLIENT_ROUTER_H

#include "base/macros_magic.h"
#include "base/peer/client_authenticator.h"
#include "proto/router_admin.h"

#include <QTimer>

namespace client {

class Router final : public QObject
{
    Q_OBJECT

public:
    explicit Router(QObject* parent = nullptr);
    ~Router() final;

    void setUserName(const QString& username);
    void setPassword(const QString& password);

    void setAutoReconnect(bool enable);
    bool isAutoReconnect() const;

    void connectToRouter(const QString& address, uint16_t port);

    void refreshSessionList();
    void stopSession(int64_t session_id);

    void refreshUserList();
    void addUser(const proto::User& user);
    void modifyUser(const proto::User& user);
    void deleteUser(int64_t entry_id);
    void disconnectPeerSession(int64_t relay_session_id, uint64_t peer_session_id);

signals:
    void sig_connecting();
    void sig_connected(const QVersionNumber& peer_version);
    void sig_disconnected(base::NetworkChannel::ErrorCode error_code);
    void sig_waitForRouter();
    void sig_waitForRouterTimeout();
    void sig_versionMismatch(const QVersionNumber& router, const QVersionNumber& client);
    void sig_accessDenied(base::Authenticator::ErrorCode error_code);
    void sig_sessionList(std::shared_ptr<proto::SessionList> session_list);
    void sig_sessionResult(std::shared_ptr<proto::SessionResult> session_result);
    void sig_userList(std::shared_ptr<proto::UserList> user_list);
    void sig_userResult(std::shared_ptr<proto::UserResult> user_result);

private slots:
    void onTcpConnected();
    void onTcpDisconnected(base::NetworkChannel::ErrorCode error_code);
    void onTcpMessageReceived(uint8_t channel_id, const QByteArray& buffer);

private:
    QTimer* timeout_timer_ = nullptr;
    QTimer* reconnect_timer_ = nullptr;
    QPointer<base::TcpChannel> channel_;
    QPointer<base::ClientAuthenticator> authenticator_;

    QString router_address_;
    uint16_t router_port_ = 0;
    QString router_username_;
    QString router_password_;

    bool auto_reconnect_ = true;
    bool reconnect_in_progress_ = false;

    DISALLOW_COPY_AND_ASSIGN(Router);
};

} // namespace client

#endif // CLIENT_ROUTER_H
