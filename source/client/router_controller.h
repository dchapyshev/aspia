//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT_ROUTER_CONTROLLER_H
#define CLIENT_ROUTER_CONTROLLER_H

#include "base/waitable_timer.h"
#include "base/net/tcp_channel.h"
#include "base/peer/authenticator.h"
#include "base/peer/host_id.h"
#include "base/peer/relay_peer.h"
#include "client/router_config.h"

namespace base {
class ClientAuthenticator;
} // namespace base

namespace client {

class RouterController
    : public base::TcpChannel::Listener,
      public base::RelayPeer::Delegate
{
public:
    enum class ErrorType
    {
        NETWORK,
        AUTHENTICATION,
        ROUTER
    };

    enum class ErrorCode
    {
        SUCCESS,
        UNKNOWN_ERROR,
        PEER_NOT_FOUND,
        ACCESS_DENIED,
        KEY_POOL_EMPTY,
        RELAY_ERROR
    };

    struct Error
    {
        ErrorType type;

        union Code
        {
            base::TcpChannel::ErrorCode network;
            base::Authenticator::ErrorCode authentication;
            ErrorCode router;
        } code;
    };

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onHostConnected(std::unique_ptr<base::TcpChannel> channel) = 0;
        virtual void onErrorOccurred(const Error& error) = 0;
    };

    RouterController(const RouterConfig& router_config,
                     std::shared_ptr<base::TaskRunner> task_runner);
    ~RouterController() override;

    void connectTo(base::HostId host_id, Delegate* delegate);

protected:
    // base::TcpChannel::Listener implementation.
    void onConnected() override;
    void onDisconnected(base::TcpChannel::ErrorCode error_code) override;
    void onMessageReceived(uint8_t channel_id, const base::ByteArray& buffer) override;
    void onMessageWritten(uint8_t channel_id, size_t pending) override;

    // base::RelayPeer::Delegate implementation.
    void onRelayConnectionReady(std::unique_ptr<base::TcpChannel> channel) override;
    void onRelayConnectionError() override;

private:
    std::shared_ptr<base::TaskRunner> task_runner_;
    std::unique_ptr<base::TcpChannel> channel_;
    std::unique_ptr<base::ClientAuthenticator> authenticator_;
    std::unique_ptr<base::RelayPeer> relay_peer_;
    RouterConfig router_config_;

    base::HostId host_id_ = base::kInvalidHostId;
    Delegate* delegate_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(RouterController);
};

} // namespace client

#endif // CLIENT_ROUTER_CONTROLLER_H
