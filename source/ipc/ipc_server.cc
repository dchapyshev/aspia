//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "base/qt_logging.h"
#include "crypto/random.h"
#include "ipc/ipc_channel.h"

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

Server::Server(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

bool Server::start()
{
    if (isStarted())
    {
        LOG(LS_WARNING) << "An attempt was start an already running server.";
        return false;
    }

    std::unique_ptr<QLocalServer> server(new QLocalServer(this));

    server->setSocketOptions(QLocalServer::OtherAccessOption);
    server->setMaxPendingConnections(25);

    connect(server.get(), &QLocalServer::newConnection, this, &Server::onNewConnection);

    QString channel_id = channel_id_;

    if (channel_id.isEmpty())
        channel_id = generateUniqueChannelId();

    if (!server->listen(channel_id))
    {
        LOG(LS_WARNING) << "listen failed: " << server->errorString();
        return false;
    }

    channel_id_ = channel_id;
    server_ = server.release();
    return true;
}

void Server::stop()
{
    if (server_)
    {
        server_->close();
        delete server_;
        emit finished();
    }
}

bool Server::isStarted() const
{
    return server_ != nullptr;
}

void Server::setChannelId(const QString& channel_id)
{
    channel_id_ = channel_id;
}

QString Server::channelId() const
{
    return channel_id_;
}

void Server::onNewConnection()
{
    if (server_->hasPendingConnections())
    {
        QLocalSocket* socket = server_->nextPendingConnection();
        emit newConnection(new Channel(Channel::Type::SERVER, socket, nullptr));
    }
}

} // namespace ipc
