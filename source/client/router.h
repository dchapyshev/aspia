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
#include "proto/router_admin.pb.h"

#include <QTimer>

namespace client {

class RouterWindowProxy;

class Router final : public QObject
{
    Q_OBJECT

public:
    explicit Router(std::shared_ptr<RouterWindowProxy> window_proxy, QObject* parent = nullptr);
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

private slots:
    void onTcpConnected();
    void onTcpDisconnected(base::NetworkChannel::ErrorCode error_code);
    void onTcpMessageReceived(uint8_t channel_id, const QByteArray& buffer);

private:
    QTimer* timeout_timer_ = nullptr;
    QTimer* reconnect_timer_ = nullptr;
    QPointer<base::TcpChannel> channel_;
    QPointer<base::ClientAuthenticator> authenticator_;
    std::shared_ptr<RouterWindowProxy> window_proxy_;

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
