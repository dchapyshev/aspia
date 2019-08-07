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

#include "ipc/ipc_server.h"

#include "crypto/random.h"
#include "ipc/ipc_channel.h"
#include "qt_base/qt_logging.h"

#include <QCoreApplication>
#include <QLocalServer>

namespace ipc {

namespace {

QString generateUniqueChannelId()
{
    static std::atomic_uint32_t last_channel_id = 0;
    uint32_t channel_id = last_channel_id++;

    return QString("%1.%2.%3")
        .arg(QCoreApplication::applicationPid())
        .arg(channel_id)
        .arg(crypto::Random::number());
}

} // namespace

bool Server::start(Delegate* delegate)
{
    DCHECK(delegate);

    if (isStarted())
    {
        DLOG(LS_WARNING) << "An attempt was start an already running server.";
        return false;
    }

    delegate_ = delegate;

    std::unique_ptr<QLocalServer> server(new QLocalServer(this));

    server->setSocketOptions(QLocalServer::OtherAccessOption);
    server->setMaxPendingConnections(25);

    connect(server.get(), &QLocalServer::newConnection, [this]()
    {
        if (server_->hasPendingConnections())
        {
            QLocalSocket* socket = server_->nextPendingConnection();
            delegate_->onNewConnection(std::unique_ptr<Channel>(new Channel(socket)));
        }
    });

    QString channel_id = channel_id_;

    if (channel_id.isEmpty())
        channel_id = generateUniqueChannelId();

    if (!server->listen(channel_id))
    {
        LOG(LS_WARNING) << "listen failed: " << server->errorString();
        return false;
    }

    channel_id_ = channel_id;
    server_ = std::move(server);
    return true;
}

bool Server::isStarted() const
{
    return server_ != nullptr;
}

void Server::setChannelId(const QString& channel_id)
{
    channel_id_ = channel_id;
}

} // namespace ipc
