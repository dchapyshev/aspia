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

#ifndef CLIENT__ROUTER_CONTROLLER_H
#define CLIENT__ROUTER_CONTROLLER_H

#include "base/waitable_timer.h"
#include "base/net/network_channel.h"
#include "base/peer/authenticator.h"
#include "base/peer/host_id.h"

namespace base {
class ClientAuthenticator;
} // namespace base

namespace client {

class RouterController : public base::NetworkChannel::Listener
{
public:
    struct RouterInfo
    {
        std::u16string address;
        uint16_t port = 0;
        std::u16string username;
        std::u16string password;
    };

    enum class ErrorType
    {
        NETWORK,
        AUTHENTICATION,
        ROUTER
    };

    enum class ErrorCode
    {
        SUCCESS,
        PEER_NOT_FOUND,
        ACCESS_DENIED
    };

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onHostConnected(std::unique_ptr<base::NetworkChannel> channel) = 0;
        virtual void onErrorOccurred(ErrorType error_type) = 0;
    };

    RouterController(const RouterInfo& router_info, std::shared_ptr<base::TaskRunner> task_runner);
    ~RouterController();

    void connectTo(base::HostId host_id, Delegate* delegate);

    base::NetworkChannel::ErrorCode networkError() const { return network_error_; }
    base::Authenticator::ErrorCode authenticationError() const { return authentication_error_; }
    ErrorCode routerError() const { return router_error_; }

protected:
    // base::NetworkChannel::Listener implementation.
    void onConnected() override;
    void onDisconnected(base::NetworkChannel::ErrorCode error_code) override;
    void onMessageReceived(const base::ByteArray& buffer) override;
    void onMessageWritten(size_t pending) override;

private:
    std::shared_ptr<base::TaskRunner> task_runner_;
    std::unique_ptr<base::NetworkChannel> channel_;
    std::unique_ptr<base::ClientAuthenticator> authenticator_;
    RouterInfo router_info_;

    base::HostId host_id_ = base::kInvalidHostId;
    Delegate* delegate_ = nullptr;

    base::NetworkChannel::ErrorCode network_error_ = base::NetworkChannel::ErrorCode::SUCCESS;
    base::Authenticator::ErrorCode authentication_error_ = base::Authenticator::ErrorCode::SUCCESS;
    ErrorCode router_error_ = ErrorCode::SUCCESS;

    DISALLOW_COPY_AND_ASSIGN(RouterController);
};

} // namespace client

#endif // CLIENT__ROUTER_CONTROLLER_H
