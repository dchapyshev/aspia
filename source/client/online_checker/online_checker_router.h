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

#ifndef CLIENT_ONLINE_CHECKER_ONLINE_CHECKER_ROUTER_H
#define CLIENT_ONLINE_CHECKER_ONLINE_CHECKER_ROUTER_H

#include <QQueue>
#include <QPointer>
#include <QTimer>

#include "base/net/tcp_channel.h"
#include "client/router_config.h"

namespace base {
class ClientAuthenticator;
class Location;
} // namespace base

namespace client {

class OnlineCheckerRouter final : public QObject
{
    Q_OBJECT

public:
    explicit OnlineCheckerRouter(const RouterConfig& router_config, QObject* parent = nullptr);
    ~OnlineCheckerRouter() final;

    struct Computer
    {
        int computer_id = -1;
        base::HostId host_id = base::kInvalidHostId;
    };
    using ComputerList = QQueue<Computer>;

    void start(const ComputerList& computers);

signals:
    void sig_checkerResult(int computer_id, bool online);
    void sig_checkerFinished();

private slots:
    void onTcpConnected();
    void onTcpDisconnected(base::TcpChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer);

private:
    void checkNextComputer();
    void onFinished(const base::Location& location);

    QPointer<base::TcpChannel> tcp_channel_;
    QPointer<base::ClientAuthenticator> authenticator_;
    QTimer timer_;
    RouterConfig router_config_;

    ComputerList computers_;
};

} // namespace client

#endif // CLIENT_ONLINE_CHECKER_ONLINE_CHECKER_ROUTER_H
