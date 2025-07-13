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

#include <QTimer>

#include "base/net/tcp_channel.h"
#include "base/peer/host_id.h"
#include "base/peer/relay_peer.h"
#include "client/router_config.h"

namespace client {

class RouterController final : public QObject
{
    Q_OBJECT

public:
    enum class ErrorType
    {
        NETWORK,
        ROUTER
    };
    Q_ENUM(ErrorType)

    enum class ErrorCode
    {
        SUCCESS,
        UNKNOWN_ERROR,
        PEER_NOT_FOUND,
        ACCESS_DENIED,
        KEY_POOL_EMPTY,
        RELAY_ERROR
    };
    Q_ENUM(ErrorCode)

    struct Error
    {
        ErrorType type;

        union Code
        {
            base::TcpChannel::ErrorCode network;
            ErrorCode router;
        } code;
    };

    explicit RouterController(const RouterConfig& router_config, QObject* parent = nullptr);
    ~RouterController() final;

    void connectTo(base::HostId host_id, base::Authenticator* authenticator, bool wait_for_host);
    base::TcpChannel* takeChannel();

signals:
    void sig_routerConnected(const QVersionNumber& version);
    void sig_hostAwaiting();
    void sig_hostConnected();
    void sig_errorOccurred(const client::RouterController::Error& error);

private slots:
    void onTcpReady();
    void onTcpErrorOccurred(base::TcpChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer);
    void onRelayConnectionReady();
    void onRelayConnectionError();

private:
    void sendConnectionRequest();
    void waitForHost();

    QPointer<base::TcpChannel> router_channel_ = nullptr;
    QTimer* status_request_timer_ = nullptr;
    QPointer<base::RelayPeer> relay_peer_ = nullptr;
    RouterConfig router_config_;

    base::HostId host_id_ = base::kInvalidHostId;
    bool wait_for_host_ = false;

    std::unique_ptr<base::Authenticator> host_authenticator_;
    base::TcpChannel* host_channel_ = nullptr;

    Q_DISABLE_COPY(RouterController)
};

} // namespace client

Q_DECLARE_METATYPE(client::RouterController::Error)

#endif // CLIENT_ROUTER_CONTROLLER_H
