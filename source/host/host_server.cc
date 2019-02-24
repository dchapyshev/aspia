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

#include <QCoreApplication>

#include "base/qt_logging.h"
#include "common/message_serialization.h"
#include "host/win/host_session_process.h"
#include "host/host_ui_server.h"
#include "host/host_settings.h"
#include "ipc/ipc_server.h"
#include "net/firewall_manager.h"
#include "net/network_channel_host.h"
#include "proto/notifier.pb.h"

namespace host {

namespace {

const char kFirewallRuleName[] = "Aspia Host Service";
const char kFirewallRuleDecription[] = "Allow incoming TCP connections";

const int kStartEvent = QEvent::User + 1;

const char* sessionTypeToString(proto::SessionType session_type)
{
    switch (session_type)
    {
        case proto::SESSION_TYPE_DESKTOP_MANAGE:
            return "Desktop Manage";

        case proto::SESSION_TYPE_DESKTOP_VIEW:
            return "Desktop View";

        case proto::SESSION_TYPE_FILE_TRANSFER:
            return "File Transfer";

        default:
            return "Unknown";
    }
}

} // namespace

HostServer::HostServer(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

HostServer::~HostServer()
{
    stop();
}

void HostServer::start()
{
    QCoreApplication::postEvent(this, new QEvent(QEvent::Type(kStartEvent)));
}

void HostServer::stop()
{
    LOG(LS_INFO) << "Stopping the server.";

    for (auto session : session_list_)
        session->stop();

    if (ui_server_)
    {
        ui_server_->stop();
        delete ui_server_;
    }

    std::unique_ptr<net::Server> network_server_deleter(network_server_);
    if (network_server_)
        network_server_->stop();

    net::FirewallManager firewall(QCoreApplication::applicationFilePath());
    if (firewall.isValid())
        firewall.deleteRuleByName(kFirewallRuleName);

    LOG(LS_INFO) << "Server is stopped.";
}

void HostServer::setSessionEvent(base::win::SessionStatus status, base::win::SessionId session_id)
{
    emit sessionEvent(status, session_id);

    if (ui_server_)
        ui_server_->setSessionEvent(status, session_id);
}

void HostServer::customEvent(QEvent* event)
{
    if (event->type() == kStartEvent)
    {
        LOG(LS_INFO) << "Starting the server.";

        if (network_server_)
        {
            DLOG(LS_WARNING) << "An attempt was start an already running server.";
            return;
        }

        Settings settings;

        if (settings.addFirewallRule())
        {
            net::FirewallManager firewall(QCoreApplication::applicationFilePath());
            if (firewall.isValid())
            {
                if (firewall.addTcpRule(kFirewallRuleName, kFirewallRuleDecription, settings.tcpPort()))
                {
                    LOG(LS_INFO) << "Rule is added to the firewall.";
                }
            }
        }

        ui_server_ = new UiServer(this);
        if (!ui_server_->start())
        {
            QCoreApplication::quit();
            return;
        }

        network_server_ = new net::Server(settings.userList(), this);

        connect(network_server_, &net::Server::newChannelReady,
                this, &HostServer::onNewConnection);

        if (!network_server_->start(settings.tcpPort()))
        {
            QCoreApplication::quit();
            return;
        }
    }

    QObject::customEvent(event);
}

void HostServer::onNewConnection()
{
    while (network_server_->hasReadyChannels())
    {
        net::ChannelHost* channel = network_server_->nextReadyChannel();
        if (!channel)
            continue;

        LOG(LS_INFO) << "New connected client: " << channel->peerAddress();

        std::unique_ptr<SessionProcess> session_process(new SessionProcess(this));

        session_process->setNetworkChannel(channel);
        session_process->setUuid(QUuid::createUuid());

        connect(this, &HostServer::sessionEvent,
                session_process.get(), &SessionProcess::setSessionEvent);

        connect(session_process.get(), &SessionProcess::finished,
                this, &HostServer::onHostFinished,
                Qt::QueuedConnection);

        if (session_process->start())
        {
            proto::notifier::ConnectEvent event;

            event.set_session_type(session_process->sessionType());
            event.set_remote_address(session_process->remoteAddress().toStdString());
            event.set_username(session_process->userName().toStdString());
            event.set_uuid(session_process->uuid().toString().toStdString());

            ui_server_->setConnectEvent(base::win::activeConsoleSessionId(), event);

            session_list_.push_back(session_process.release());
        }
    }
}

void HostServer::onHostFinished(SessionProcess* host)
{
    LOG(LS_INFO) << sessionTypeToString(host->sessionType())
                 << " session is finished for " << host->userName();

    for (auto it = session_list_.begin(); it != session_list_.end(); ++it)
    {
        if (*it != host)
            continue;

        session_list_.erase(it);

        std::unique_ptr<SessionProcess> host_deleter(host);

        ui_server_->setDisconnectEvent(
            base::win::activeConsoleSessionId(), host->uuid().toString().toStdString());
        break;
    }
}

} // namespace host
