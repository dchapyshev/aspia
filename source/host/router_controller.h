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

#ifndef HOST_ROUTER_CONTROLLER_H
#define HOST_ROUTER_CONTROLLER_H

#include "base/net/tcp_channel.h"
#include "base/peer/host_id.h"
#include "base/peer/relay_peer_manager.h"
#include "proto/host_internal.pb.h"

#include <queue>

#include <QTimer>

namespace base {
class ClientAuthenticator;
} // namespace base

namespace host {

class RouterController final
    : public QObject,
      public base::RelayPeerManager::Delegate
{
    Q_OBJECT

public:
    explicit RouterController(QObject* parent = nullptr);
    ~RouterController() final;

    struct RouterInfo
    {
        QString address;
        uint16_t port = 0;
        QByteArray public_key;
    };

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onRouterStateChanged(const proto::internal::RouterState& router_state) = 0;
        virtual void onHostIdAssigned(const QString& username, base::HostId host_id) = 0;
        virtual void onClientConnected(std::unique_ptr<base::TcpChannel> channel) = 0;
    };

    void start(const RouterInfo& router_info, Delegate* delegate);

    void hostIdRequest(const QString& session_name);
    void resetHostId(base::HostId host_id);

    const QString& address() const { return router_info_.address; }
    uint16_t port() const { return router_info_.port; }
    const QByteArray& publicKey() const { return router_info_.public_key; }

private slots:
    void onTcpConnected();
    void onTcpDisconnected(base::NetworkChannel::ErrorCode error_code);
    void onTcpMessageReceived(uint8_t channel_id, const QByteArray& buffer);

protected:
    // base::RelayPeerManager::Delegate implementation.
    void onNewPeerConnected(std::unique_ptr<base::TcpChannel> channel) final;

private:
    void connectToRouter();
    void delayedConnectToRouter();
    void routerStateChanged(proto::internal::RouterState::State state);
    static const char* routerStateToString(proto::internal::RouterState::State state);

    Delegate* delegate_ = nullptr;

    std::unique_ptr<base::TcpChannel> channel_;
    std::unique_ptr<base::ClientAuthenticator> authenticator_;
    std::unique_ptr<base::RelayPeerManager> peer_manager_;
    QTimer reconnect_timer_;
    RouterInfo router_info_;

    std::queue<QString> pending_id_requests_;

    DISALLOW_COPY_AND_ASSIGN(RouterController);
};

} // namespace host

#endif // HOST_ROUTER_CONTROLLER_H
