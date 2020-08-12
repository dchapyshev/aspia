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

#ifndef HOST__ROUTER_CONTROLLER_H
#define HOST__ROUTER_CONTROLLER_H

#include "base/macros_magic.h"
#include "base/waitable_timer.h"
#include "base/net/network_channel.h"
#include "base/peer/host_id.h"

namespace base {
class ClientAuthenticator;
} // namespace peer

namespace host {

class RouterController : public base::NetworkChannel::Listener
{
public:
    explicit RouterController(std::shared_ptr<base::TaskRunner> task_runner);
    ~RouterController();

    struct RouterInfo
    {
        std::u16string address;
        uint16_t port = 0;
        base::ByteArray public_key;
        std::u16string username;
        std::u16string password;
        base::ByteArray host_key;
    };

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onRouterConnected() = 0;
        virtual void onRouterDisconnected(base::NetworkChannel::ErrorCode error_code) = 0;
        virtual void onHostIdAssigned(base::HostId host_id, const base::ByteArray& host_key) = 0;
        virtual void onClientConnected(std::unique_ptr<base::NetworkChannel> channel) = 0;
    };

    void start(const RouterInfo& router_info, Delegate* delegate);

    const std::u16string& address() const { return router_info_.address; }
    uint16_t port() const { return router_info_.port; }
    const base::ByteArray& publicKey() const { return router_info_.public_key; }
    const base::ByteArray& hostKey() const { return router_info_.host_key; }
    base::HostId hostId() const { return host_id_; }

protected:
    // base::NetworkChannel::Listener implementation.
    void onConnected() override;
    void onDisconnected(base::NetworkChannel::ErrorCode error_code) override;
    void onMessageReceived(const base::ByteArray& buffer) override;
    void onMessageWritten(size_t pending) override;

private:
    void connectToRouter();
    void delayedConnectToRouter();

    Delegate* delegate_ = nullptr;

    std::shared_ptr<base::TaskRunner> task_runner_;
    std::unique_ptr<base::NetworkChannel> channel_;
    std::unique_ptr<base::ClientAuthenticator> authenticator_;
    base::WaitableTimer reconnect_timer_;
    base::HostId host_id_ = base::kInvalidHostId;
    RouterInfo router_info_;

    DISALLOW_COPY_AND_ASSIGN(RouterController);
};

} // namespace host

#endif // HOST__ROUTER_CONTROLLER_H
