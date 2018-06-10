//
// PROJECT:         Aspia
// FILE:            network/network_server.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "network/network_server.h"

#include <QDebug>

#include "network/network_channel.h"

namespace aspia {

NetworkServer::NetworkServer(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

NetworkServer::~NetworkServer() = default;

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

    for (auto it = pending_channels_.begin(); it != pending_channels_.end(); ++it)
    {
        NetworkChannel* network_channel = *it;

        if (network_channel)
            network_channel->stop();
    }

    for (auto it = ready_channels_.begin(); it != ready_channels_.end(); ++it)
    {
        NetworkChannel* network_channel = *it;

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

NetworkChannel* NetworkServer::nextReadyChannel()
{
    if (ready_channels_.isEmpty())
        return nullptr;

    NetworkChannel* network_channel = ready_channels_.front();
    ready_channels_.pop_front();
    return network_channel;
}

void NetworkServer::onNewConnection()
{
    QTcpSocket* socket = tcp_server_->nextPendingConnection();
    if (!socket)
        return;

    NetworkChannel* network_channel =
        new NetworkChannel(NetworkChannel::ServerChannel, socket, this);

    connect(network_channel, &NetworkChannel::connected,
            this, &NetworkServer::onChannelReady);

    pending_channels_.push_back(network_channel);

    // Start connection (key exchange).
    network_channel->onConnected();
}

void NetworkServer::onChannelReady()
{
    auto it = pending_channels_.begin();

    while (it != pending_channels_.end())
    {
        NetworkChannel* network_channel = *it;

        if (!network_channel)
        {
            it = pending_channels_.erase(it);
        }
        else if (network_channel->channelState() == NetworkChannel::Encrypted)
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
