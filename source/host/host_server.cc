//
// PROJECT:         Aspia
// FILE:            host/host_server.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_server.h"

#include <QDebug>

#include "host/win/host.h"
#include "host/host_user_authorizer.h"
#include "network/network_channel.h"

namespace aspia {

HostServer::HostServer(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

HostServer::~HostServer()
{
    stop();
}

bool HostServer::start(int port)
{
    if (!network_server_.isNull())
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

    network_server_ = new NetworkServer(this);

    connect(network_server_, &NetworkServer::newChannelConnected,
            this, &HostServer::onNewConnection);

    if (!network_server_->start(port))
        return false;

    return true;
}

void HostServer::stop()
{
    if (!network_server_.isNull())
    {
        network_server_->stop();
        delete network_server_;
    }

    user_list_.clear();
}

void HostServer::setSessionChanged(quint32 event, quint32 session_id)
{
    emit sessionChanged(event, session_id);
}

void HostServer::onNewConnection()
{
    while (network_server_->hasPendingChannels())
    {
        NetworkChannel* channel = network_server_->nextPendingChannel();
        if (!channel)
            continue;

        HostUserAuthorizer* authorizer = new HostUserAuthorizer(this);

        authorizer->setNetworkChannel(channel);
        authorizer->setUserList(user_list_);

        connect(authorizer, &HostUserAuthorizer::finished,
                this, &HostServer::onAuthorizationFinished);

        authorizer->start();
    }
}

void HostServer::onAuthorizationFinished(HostUserAuthorizer* authorizer)
{
    QScopedPointer<HostUserAuthorizer> authorizer_deleter(authorizer);

    if (authorizer->status() != proto::auth::STATUS_SUCCESS)
        return;

    Host* host = new Host(authorizer->sessionType(), authorizer->takeNetworkChannel(), this);

    connect(this, &HostServer::sessionChanged, host, &Host::sessionChanged);
    connect(host, &Host::finished, this, &HostServer::onHostFinished);

    host->start();
}

void HostServer::onHostFinished(Host* host)
{
    delete host;
}

} // namespace aspia
