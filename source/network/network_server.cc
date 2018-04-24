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
            [this](QAbstractSocket::SocketError /*error */)
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
    tcp_server_->close();
    delete tcp_server_;
}

bool NetworkServer::hasPendingChannels() const
{
    return !pending_channels_.isEmpty();
}

NetworkChannel* NetworkServer::nextPendingChannel()
{
    if (pending_channels_.isEmpty())
        return nullptr;

    NetworkChannel* channel = pending_channels_.front();
    pending_channels_.pop_front();
    return channel;
}

void NetworkServer::onNewConnection()
{
    QTcpSocket* socket = tcp_server_->nextPendingConnection();
    if (!socket)
        return;

    // Disable the Nagle algorithm for the socket.
    socket->setSocketOption(QTcpSocket::LowDelayOption, 1);

    pending_channels_.push_back(
        new NetworkChannel(NetworkChannel::ServerChannel, socket, this));

    emit newChannelConnected();
}

} // namespace aspia
