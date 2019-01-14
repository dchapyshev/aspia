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

#include "base/logging.h"
#include "network/network_channel_host.h"

namespace net {

Server::Server(const SrpUserList& user_list, QObject* parent)
    : QObject(parent),
      user_list_(user_list)
{
    // Nothing
}

bool Server::start(uint16_t port)
{
    if (!tcp_server_.isNull())
    {
        LOG(LS_WARNING) << "Server already started";
        return false;
    }

    tcp_server_ = new QTcpServer(this);

    connect(tcp_server_, &QTcpServer::newConnection, this, &Server::onNewConnection);
    connect(tcp_server_, &QTcpServer::acceptError,
            [this](QAbstractSocket::SocketError /* error */)
    {
        LOG(LS_WARNING) << "accept error: " << tcp_server_->errorString().toStdString();
        return;
    });

    if (!tcp_server_->listen(QHostAddress::Any, port))
    {
        LOG(LS_WARNING) << "listen failed: " << tcp_server_->errorString().toStdString();
        return false;
    }

    return true;
}

void Server::stop()
{
    if (tcp_server_.isNull())
    {
        LOG(LS_WARNING) << "Server already stopped";
        return;
    }

    for (auto it = pending_channels_.constBegin(); it != pending_channels_.constEnd(); ++it)
    {
        ChannelHost* network_channel = *it;

        if (network_channel)
            network_channel->stop();
    }

    for (auto it = ready_channels_.constBegin(); it != ready_channels_.constEnd(); ++it)
    {
        ChannelHost* network_channel = *it;

        if (network_channel)
            network_channel->stop();
    }

    pending_channels_.clear();
    ready_channels_.clear();

    tcp_server_->close();
    delete tcp_server_;
}

bool Server::hasReadyChannels() const
{
    return !ready_channels_.isEmpty();
}

ChannelHost* Server::nextReadyChannel()
{
    if (ready_channels_.isEmpty())
        return nullptr;

    ChannelHost* network_channel = ready_channels_.front();
    ready_channels_.pop_front();
    return network_channel;
}

void Server::onNewConnection()
{
    QTcpSocket* socket = tcp_server_->nextPendingConnection();
    if (!socket)
        return;

    ChannelHost* host_channel = new ChannelHost(socket, user_list_, this);
    connect(host_channel, &ChannelHost::keyExchangeFinished, this, &Server::onChannelReady);
    pending_channels_.push_back(host_channel);

    // Start key exchange.
    host_channel->startKeyExchange();
}

void Server::onChannelReady()
{
    auto it = pending_channels_.begin();

    while (it != pending_channels_.end())
    {
        ChannelHost* network_channel = *it;

        if (!network_channel)
        {
            it = pending_channels_.erase(it);
        }
        else if (network_channel->channelState() == Channel::ChannelState::ENCRYPTED)
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

} // namespace net
