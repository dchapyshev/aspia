//
// PROJECT:         Aspia
// FILE:            network/server.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "network/server.h"

#include <QDebug>
#include <QTcpServer>

#include "host/host.h"
#include "host/host_user_authorizer.h"
#include "network/network_channel.h"

namespace aspia {

Server::Server(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

Server::~Server()
{
    stop();
}

bool Server::start(int port)
{
    if (!server_.isNull())
    {
        qWarning("An attempt was start an already running server.");
        return false;
    }

    user_list_ = readUserList();
    if (user_list_.isEmpty())
    {
        qWarning("Empty user list");
        return false;
    }

    server_ = new QTcpServer(this);

    connect(server_, &QTcpServer::newConnection, this, &Server::onNewConnection);
    connect(server_, &QTcpServer::acceptError, [this](QAbstractSocket::SocketError /*error */)
    {
        qWarning() << "accept error: " << server_->errorString();
        return;
    });

    if (!server_->listen(QHostAddress::Any, port))
    {
        qWarning() << "listen failed: " << server_->errorString();
        return false;
    }

    return true;
}

void Server::stop()
{
    if (!server_.isNull())
    {
        server_->close();
        delete server_;
    }

    user_list_.clear();
}

void Server::setSessionChanged(quint32 event, quint32 session_id)
{
    emit sessionChanged(event, session_id);
}

void Server::onNewConnection()
{
    while (server_->hasPendingConnections())
    {
        QTcpSocket* socket = server_->nextPendingConnection();

        // Disable the Nagle algorithm for the socket.
        socket->setSocketOption(QTcpSocket::LowDelayOption, 1);

        HostUserAuthorizer* authorizer = new HostUserAuthorizer(this);

        authorizer->setNetworkChannel(new NetworkChannel(socket, nullptr));
        authorizer->setUserList(user_list_);

        // If successful authorization, create a session.
        connect(authorizer, &HostUserAuthorizer::finished, this, &Server::onAuthorizationFinished);

        authorizer->start();
    }
}

void Server::onAuthorizationFinished(HostUserAuthorizer* authorizer)
{
    QScopedPointer<HostUserAuthorizer> authorizer_deleter(authorizer);

    if (authorizer->status() != proto::auth::STATUS_SUCCESS)
        return;

    Host* host = new Host(authorizer->sessionType(), authorizer->takeNetworkChannel(), this);

    connect(this, &Server::sessionChanged, host, &Host::sessionChanged);
    connect(host, &Host::finished, this, &Server::onHostFinished);

    host->start();
}

void Server::onHostFinished(Host* host)
{
    delete host;
}

} // namespace aspia
