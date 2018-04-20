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
    if (!QSslSocket::supportsSsl())
    {
        qWarning("SSL/TLS not supported");
        return false;
    }

    QSslCertificate local_certificate;
    QSslKey private_key;

    if (!SslServer::generateCertificateAndKey(&local_certificate, &private_key))
    {
        qWarning("TLS certificate or key not generated");
        return false;
    }

    ssl_server_ = new SslServer(this);

    ssl_server_->setLocalCertificate(local_certificate);
    ssl_server_->setPrivateKey(private_key);

    connect(ssl_server_, &SslServer::newSslConnection, this, &NetworkServer::onNewSslConnection);
    connect(ssl_server_, &SslServer::acceptError,
            [this](QAbstractSocket::SocketError /*error */)
    {
        qWarning() << "accept error: " << ssl_server_->errorString();
        return;
    });

    if (!ssl_server_->listen(QHostAddress::Any, port))
    {
        qWarning() << "listen failed: " << ssl_server_->errorString();
        return false;
    }

    return true;
}

void NetworkServer::stop()
{
    ssl_server_->close();
    delete ssl_server_;
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

void NetworkServer::onNewSslConnection()
{
    while (ssl_server_->hasPendingConnections())
    {
        QSslSocket* socket = reinterpret_cast<QSslSocket*>(ssl_server_->nextPendingConnection());
        if (!socket)
            continue;

        // Disable the Nagle algorithm for the socket.
        socket->setSocketOption(QSslSocket::LowDelayOption, 1);

        pending_channels_.push_back(
            new NetworkChannel(NetworkChannel::ServerChannel, socket, this));

        emit newChannelConnected();
    }
}

} // namespace aspia
