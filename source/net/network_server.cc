//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "net/network_server.h"

#include "net/network_channel.h"
#include "qt_base/qt_logging.h"

namespace net {

bool Server::start(uint16_t port, Delegate* delegate)
{
    DCHECK(delegate);

    if (tcp_server_)
    {
        DLOG(LS_WARNING) << "Server already started";
        return false;
    }

    tcp_server_ = std::make_unique<QTcpServer>();
    delegate_ = delegate;

    connect(tcp_server_.get(), &QTcpServer::newConnection, [this]()
    {
        while (tcp_server_->hasPendingConnections())
        {
            QTcpSocket* socket = tcp_server_->nextPendingConnection();
            if (!socket)
                continue;

            delegate_->onNewConnection(std::unique_ptr<Channel>(new Channel(socket)));
        }
    });

    connect(tcp_server_.get(), &QTcpServer::acceptError,
            [this](QAbstractSocket::SocketError /* error */)
    {
        LOG(LS_WARNING) << "accept error: " << tcp_server_->errorString();
        return;
    });

    if (!tcp_server_->listen(QHostAddress::Any, port))
    {
        LOG(LS_WARNING) << "listen failed: " << tcp_server_->errorString();
        return false;
    }

    return true;
}

} // namespace net
