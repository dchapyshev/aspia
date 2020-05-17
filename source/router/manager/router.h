//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ROUTER__MANAGER__ROUTER_H
#define ROUTER__MANAGER__ROUTER_H

#include "base/macros_magic.h"
#include "net/channel.h"
#include "net/client_authenticator.h"
#include "proto/router.pb.h"

namespace base {
class TaskRunner;
} // namespace base

namespace router {

class RouterWindowProxy;

class Router : public net::Channel::Listener
{
public:
    Router(std::shared_ptr<RouterWindowProxy> window_proxy,
           std::shared_ptr<base::TaskRunner> io_task_runner);
    ~Router();

    void setPublicKey(const base::ByteArray& public_key);
    void setUserName(std::u16string_view user_name);
    void setPassword(std::u16string_view password);

    void connectToRouter(std::u16string_view address, uint16_t port);

    void refreshPeerList();
    void disconnectPeer(uint64_t peer_id);
    void refreshProxyList();
    void refreshUserList();
    void addUser(const proto::User& user);
    void modifyUser(const proto::User& user);
    void deleteUser(int64_t entry_id);

protected:
    // net::Channel::Listener implementation.
    void onConnected() override;
    void onDisconnected(net::Channel::ErrorCode error_code) override;
    void onMessageReceived(const base::ByteArray& buffer) override;
    void onMessageWritten(size_t pending) override;

private:
    std::shared_ptr<base::TaskRunner> io_task_runner_;
    std::unique_ptr<net::Channel> channel_;
    std::unique_ptr<net::ClientAuthenticator> authenticator_;
    std::shared_ptr<RouterWindowProxy> window_proxy_;

    DISALLOW_COPY_AND_ASSIGN(Router);
};

} // namespace router

#endif // ROUTER__MANAGER__ROUTER_H
