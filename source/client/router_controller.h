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

#ifndef CLIENT_ROUTER_CONTROLLER_H
#define CLIENT_ROUTER_CONTROLLER_H

#include "base/net/tcp_channel.h"
#include "base/peer/authenticator.h"
#include "base/peer/host_id.h"
#include "base/peer/relay_peer.h"
#include "client/router_config.h"

#include <QTimer>

namespace base {
class ClientAuthenticator;
} // namespace base

namespace client {

class RouterController final : public QObject
{
    Q_OBJECT

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

    explicit RouterController(const RouterConfig& router_config, QObject* parent = nullptr);
    ~RouterController() final;

    void connectTo(base::HostId host_id, bool wait_for_host);
    base::TcpChannel* takeChannel();

signals:
    void sig_routerConnected(const QVersionNumber& version);
    void sig_hostAwaiting();
    void sig_hostConnected();
    void sig_errorOccurred(const client::RouterController::Error& error);

private slots:
    void onTcpConnected();
    void onTcpDisconnected(base::NetworkChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer);
    void onRelayConnectionReady();
    void onRelayConnectionError();

private:
    void sendConnectionRequest();
    void waitForHost();

    QPointer<base::TcpChannel> router_channel_ = nullptr;
    QPointer<base::ClientAuthenticator> authenticator_ = nullptr;
    QTimer* status_request_timer_ = nullptr;
    QPointer<base::RelayPeer> relay_peer_ = nullptr;
    RouterConfig router_config_;

    base::HostId host_id_ = base::kInvalidHostId;
    bool wait_for_host_ = false;

    std::unique_ptr<base::TcpChannel> host_channel_;

    DISALLOW_COPY_AND_ASSIGN(RouterController);
};

} // namespace client

Q_DECLARE_METATYPE(client::RouterController::Error)

#endif // CLIENT_ROUTER_CONTROLLER_H
