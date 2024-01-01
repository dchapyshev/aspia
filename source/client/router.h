//
// Aspia Project
// Copyright (C) 2016-2024 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "base/waitable_timer.h"
#include "base/peer/client_authenticator.h"
#include "base/peer/host_id.h"
#include "proto/router_admin.pb.h"

namespace base {
class TaskRunner;
} // namespace base

namespace client {

class RouterWindowProxy;

class Router : public base::TcpChannel::Listener
{
public:
    Router(std::shared_ptr<RouterWindowProxy> window_proxy,
           std::shared_ptr<base::TaskRunner> io_task_runner);
    ~Router() override;

    void setUserName(std::u16string_view username);
    void setPassword(std::u16string_view password);

    void setAutoReconnect(bool enable);
    bool isAutoReconnect() const;

    void connectToRouter(std::u16string_view address, uint16_t port);

    void refreshSessionList();
    void stopSession(int64_t session_id);

    void refreshUserList();
    void addUser(const proto::User& user);
    void modifyUser(const proto::User& user);
    void deleteUser(int64_t entry_id);
    void disconnectPeerSession(int64_t relay_session_id, uint64_t peer_session_id);

protected:
    // net::TcpChannel::Listener implementation.
    void onTcpConnected() override;
    void onTcpDisconnected(base::NetworkChannel::ErrorCode error_code) override;
    void onTcpMessageReceived(uint8_t channel_id, const base::ByteArray& buffer) override;
    void onTcpMessageWritten(uint8_t channel_id, base::ByteArray&& buffer, size_t pending) override;

private:
    std::shared_ptr<base::TaskRunner> io_task_runner_;
    std::unique_ptr<base::WaitableTimer> timeout_timer_;
    std::unique_ptr<base::WaitableTimer> reconnect_timer_;
    std::unique_ptr<base::TcpChannel> channel_;
    std::unique_ptr<base::ClientAuthenticator> authenticator_;
    std::shared_ptr<RouterWindowProxy> window_proxy_;

    std::u16string router_address_;
    uint16_t router_port_ = 0;
    std::u16string router_username_;
    std::u16string router_password_;

    bool auto_reconnect_ = true;
    bool reconnect_in_progress_ = false;

    DISALLOW_COPY_AND_ASSIGN(Router);
};

} // namespace client

#endif // CLIENT_ROUTER_H
