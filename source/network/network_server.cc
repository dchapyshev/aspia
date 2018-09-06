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

#include "network/network_server.h"

#include <QDebug>

#include "network/network_channel_host.h"

namespace aspia {

NetworkServer::NetworkServer(std::shared_ptr<proto::SrpUserList>& user_list, QObject* parent)
    : QObject(parent),
      user_list_(user_list)
{
    Q_ASSERT(user_list_);
}

bool NetworkServer::start(int port)
{
    if (!tcp_server_.isNull())
    {
        qWarning("Server already started");
        return false;
    }

    tcp_server_ = new QTcpServer(this);

    connect(tcp_server_, &QTcpServer::newConnection, this, &NetworkServer::onNewConnection);
    connect(tcp_server_, &QTcpServer::acceptError,
            [this](QAbstractSocket::SocketError /* error */)
    {
        qWarning() << "accept error: " << tcp_server_->errorString();
        return;
    });

    if (!tcp_server_->listen(QHostAddress::Any, port))
    {
        qWarning() << "listen failed: " << tcp_server_->errorString();
        return false;
    }

    return true;
}

void NetworkServer::stop()
{
    if (tcp_server_.isNull())
    {
        qWarning("Server already stopped");
        return;
    }

    for (auto it = pending_channels_.constBegin(); it != pending_channels_.constEnd(); ++it)
    {
        NetworkChannelHost* network_channel = *it;

        if (network_channel)
            network_channel->stop();
    }

    for (auto it = ready_channels_.constBegin(); it != ready_channels_.constEnd(); ++it)
    {
        NetworkChannelHost* network_channel = *it;

        if (network_channel)
            network_channel->stop();
    }

    pending_channels_.clear();
    ready_channels_.clear();

    tcp_server_->close();
    delete tcp_server_;
}

bool NetworkServer::hasReadyChannels() const
{
    return !ready_channels_.isEmpty();
}

NetworkChannelHost* NetworkServer::nextReadyChannel()
{
    if (ready_channels_.isEmpty())
        return nullptr;

    NetworkChannelHost* network_channel = ready_channels_.front();
    ready_channels_.pop_front();
    return network_channel;
}

void NetworkServer::onNewConnection()
{
    QTcpSocket* socket = tcp_server_->nextPendingConnection();
    if (!socket)
        return;

    NetworkChannelHost* host_channel = new NetworkChannelHost(socket, user_list_, this);

    connect(host_channel, &NetworkChannelHost::keyExchangeFinished,
            this, &NetworkServer::onChannelReady);

    pending_channels_.push_back(host_channel);

    // Start key exchange.
    host_channel->startKeyExchange();
}

void NetworkServer::onChannelReady()
{
    auto it = pending_channels_.begin();

    while (it != pending_channels_.end())
    {
        NetworkChannelHost* network_channel = *it;

        if (!network_channel)
        {
            it = pending_channels_.erase(it);
        }
        else if (network_channel->channelState() == NetworkChannel::ChannelState::ENCRYPTED)
        {
            it = pending_channels_.erase(it);

            ready_channels_.push_back(network_channel);
            emit newChannelReady();
        }
        else
        {
            ++it;
        }
    }
}

} // namespace aspia
