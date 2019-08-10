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

#include "host/host_server.h"

#include "host/client_session.h"
#include "host/host_authenticator_manager.h"
#include "host/user.h"
#include "net/firewall_manager.h"
#include "net/network_channel.h"
#include "qt_base/qt_logging.h"

#include <QFileSystemWatcher>

namespace host {

namespace {

const char kFirewallRuleName[] = "Aspia Host Service";
const char kFirewallRuleDecription[] = "Allow incoming TCP connections";

} // namespace

Server::Server() = default;

Server::~Server()
{
    LOG(LS_INFO) << "Stopping the server";

    settings_watcher_.reset();
    network_server_.reset();

    net::FirewallManager firewall(QCoreApplication::applicationFilePath());
    if (firewall.isValid())
    {
        firewall.deleteRuleByName(kFirewallRuleName);
    }

    LOG(LS_INFO) << "Server is stopped";
}

void Server::start()
{
    if (network_server_)
    {
        DLOG(LS_WARNING) << "An attempt was start an already running server";
        return;
    }

    LOG(LS_INFO) << "Starting the server";

    net::FirewallManager firewall(QCoreApplication::applicationFilePath());
    if (firewall.isValid())
    {
        if (firewall.addTcpRule(kFirewallRuleName, kFirewallRuleDecription, settings_.tcpPort()))
        {
            LOG(LS_INFO) << "Rule is added to the firewall";
        }
    }

    network_server_ = std::make_unique<net::Server>();
    if (!network_server_->start(settings_.tcpPort(), this))
    {
        QCoreApplication::quit();
        return;
    }

    LOG(LS_INFO) << "Network server is started successfully";

    authenticator_manager_ = std::make_unique<AuthenticatorManager>(this);
    authenticator_manager_->setUserList(std::make_shared<UserList>(settings_.userList()));

    settings_watcher_ = std::make_unique<QFileSystemWatcher>();

    QObject::connect(settings_watcher_.get(), &QFileSystemWatcher::fileChanged, [this]()
    {
        // Synchronize the parameters from the file.
        settings_.sync();

        // Updating the list of users.
        authenticator_manager_->setUserList(std::make_shared<UserList>(settings_.userList()));
    });

    settings_watcher_->addPath(settings_.filePath());
}

void Server::setSessionEvent(base::win::SessionStatus status, base::win::SessionId session_id)
{
    // TODO
}

void Server::onNewConnection(std::unique_ptr<net::Channel> channel)
{
    LOG(LS_INFO) << "New connection from: " << channel->peerAddress();

    DCHECK(authenticator_manager_);
    authenticator_manager_->addNewChannel(std::move(channel));
}

void Server::onNewSession(std::unique_ptr<ClientSession> session)
{
    LOG(LS_INFO) << "Client connected";
    LOG(LS_INFO) << "ID: " << session->id();
    LOG(LS_INFO) << "Username: " << session->userName();
    LOG(LS_INFO) << "Peer IP: " << session->peerAddress();
}

} // namespace host
