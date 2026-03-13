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

#ifndef CLIENT_ROUTER_MANAGER_H
#define CLIENT_ROUTER_MANAGER_H

#include <QObject>

#include "base/net/tcp_channel.h"
#include "base/peer/host_id.h"
#include "base/peer/relay_peer.h"
#include "client/router_config.h"

class QTimer;

namespace client {

class RouterManager final : public QObject
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

    explicit RouterManager(const RouterConfig& router_config, QObject* parent = nullptr);
    ~RouterManager() final;

    void connectTo(base::HostId host_id, base::Authenticator* authenticator, bool wait_for_host);
    base::TcpChannel* takeChannel();

    QString stunHost() const { return stun_host_; }
    quint16 stunPort() const { return stun_port_; }

signals:
    void sig_routerConnected(const QVersionNumber& version);
    void sig_hostAwaiting();
    void sig_hostConnected();
    void sig_errorOccurred(const client::RouterManager::Error& error);

private slots:
    void onTcpReady();
    void onTcpErrorOccurred(base::TcpChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer);
    void onRelayConnectionReady();
    void onRelayConnectionError();

private:
    void sendConnectionRequest();
    void waitForHost();

    base::TcpChannel* router_channel_ = nullptr;
    QTimer* status_request_timer_ = nullptr;
    base::RelayPeer* relay_peer_ = nullptr;
    RouterConfig router_config_;

    QString stun_host_;
    quint16 stun_port_ = 0;

    base::HostId host_id_ = base::kInvalidHostId;
    bool wait_for_host_ = false;

    std::unique_ptr<base::Authenticator> host_authenticator_;
    base::TcpChannel* host_channel_ = nullptr;

    Q_DISABLE_COPY_MOVE(RouterManager)
};

} // namespace client

Q_DECLARE_METATYPE(client::RouterManager::Error)

#endif // CLIENT_ROUTER_MANAGER_H
