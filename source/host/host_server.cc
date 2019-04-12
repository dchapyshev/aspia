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

#include "base/guid.h"
#include "base/qt_logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/win/session_info.h"
#include "common/session_type.h"
#include "host/power_save_blocker.h"
#include "host/win/host_session_process.h"
#include "net/firewall_manager.h"
#include "net/network_channel_host.h"

#include <QCoreApplication>
#include <QFileSystemWatcher>

namespace host {

namespace {

const char kFirewallRuleName[] = "Aspia Host Service";
const char kFirewallRuleDecription[] = "Allow incoming TCP connections";

const int kStartEvent = QEvent::User + 1;

} // namespace

HostServer::HostServer(QObject* parent)
    : QObject(parent)
{
    connect(&dettach_timer_, &DettachTimer::dettachSession, [this](base::win::SessionId session_id)
    {
        for (auto it = sessions_.begin(); it != sessions_.end();)
        {
            SessionProcess* process = it->get();

            if (process->sessionId() == session_id)
                it = sessions_.erase(it);
            else
                ++it;
        }
    });
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
    if (state_ == State::STOPPED || state_ == State::STOPPING)
        return;

    LOG(LS_INFO) << "Stopping the server";
    state_ = State::STOPPING;

    settings_watcher_.reset();
    sessions_.clear();
    ui_server_.reset();
    network_server_.reset();

    net::FirewallManager firewall(QCoreApplication::applicationFilePath());
    if (firewall.isValid())
        firewall.deleteRuleByName(kFirewallRuleName);

    LOG(LS_INFO) << "Server is stopped";
    state_ = State::STOPPED;
}

void HostServer::setSessionEvent(base::win::SessionStatus status, base::win::SessionId session_id)
{
    for (const auto& session : sessions_)
        session->setSessionEvent(status, session_id);

    if (ui_server_)
        ui_server_->setSessionEvent(status, session_id);

    if (status == base::win::SessionStatus::SESSION_LOGOFF)
        dettach_timer_.sessionLogoff(session_id);
}

void HostServer::stopSession(const std::string& uuid)
{
    for (const auto& session : sessions_)
    {
        if (session->uuid() == uuid)
        {
            session->stop();
            break;
        }
    }
}

void HostServer::customEvent(QEvent* event)
{
    if (event->type() == kStartEvent)
        startServer();

    QObject::customEvent(event);
}

void HostServer::onNewConnection()
{
    while (network_server_->hasReadyChannels())
    {
        net::ChannelHost* channel = network_server_->nextReadyChannel();
        if (!channel)
            continue;

        if (!ui_server_ || ui_server_->state() != UiServer::State::STARTED)
        {
            LOG(LS_ERROR) << "UI server not started. Connection aborted";
            channel->deleteLater();
            return;
        }

        base::win::SessionId session_id = base::win::kInvalidSessionId;
        QString user_name = channel->userName();

        if (user_name.front() != '#')
        {
            session_id = base::win::activeConsoleSessionId();
        }
        else
        {
            user_name.remove(0, 1);

            bool ok;
            session_id = user_name.toULong(&ok);
            if (!ok)
                session_id = base::win::kInvalidSessionId;
        }

        QString peer_address = channel->peerAddress();

        base::win::SessionInfo session_info(session_id);
        if (!session_info.isValid())
        {
            LOG(LS_ERROR) << "Unable to determine session state. Connection aborted";
            channel->deleteLater();
            return;
        }

        switch (session_info.connectState())
        {
            case WTSConnected:
                LOG(LS_INFO) << "Session exists, but there are no logged in users";
                break;

            default:
            {
                if (!ui_server_->hasUiForSession(session_id))
                {
                    LOG(LS_INFO) << "UI process for session " << session_id << " is not running. "
                                    "Connection from " << peer_address << " aborted";
                    channel->deleteLater();
                    continue;
                }
            }
            break;
        }

        LOG(LS_INFO) << "New connected client: " << peer_address
                     << " (SID: " << session_id << ")";

        std::unique_ptr<SessionProcess> session_process = std::make_unique<SessionProcess>();

        session_process->setNetworkChannel(channel);
        session_process->setUuid(base::Guid::create().toStdString());

        connect(session_process.get(), &SessionProcess::finished,
                this, &HostServer::onSessionFinished,
                Qt::QueuedConnection);

        if (session_process->start(session_id))
        {
            sessions_.emplace_front(std::move(session_process));
            sendConnectEvent(sessions_.front().get());

            if (!power_save_blocker_)
                power_save_blocker_.reset(new PowerSaveBlocker());
        }
    }
}

void HostServer::onUiProcessEvent(UiServer::EventType event, base::win::SessionId session_id)
{
    for (const auto& session : sessions_)
    {
        if (session->sessionId() != session_id)
            continue;

        if (event == UiServer::EventType::CONNECTED)
        {
            sendConnectEvent(session.get());

            if (dettach_timer_.sessionConnect(session_id))
                session->attachSession(session_id);
        }
        else
        {
            DCHECK(event == UiServer::EventType::DISCONNECTED);

            session->dettachSession();
            dettach_timer_.sessionDisconnect(session_id);
        }
    }
}

void HostServer::onSessionFinished()
{
    if (state_ == State::STOPPING)
        return;

    for (auto it = sessions_.begin(); it != sessions_.end();)
    {
        SessionProcess* session_process = it->get();

        if (session_process->state() == SessionProcess::State::STOPPED)
        {
            LOG(LS_INFO) << common::sessionTypeToString(session_process->sessionType())
                         << " session is finished for " << session_process->userName();

            if (ui_server_)
            {
                ui_server_->setDisconnectEvent(session_process->sessionId(),
                                               session_process->uuid());
            }

            it = sessions_.erase(it);
        }
        else
        {
            ++it;
        }
    }

    if (sessions_.empty())
        power_save_blocker_.reset();
}

void HostServer::reloadUsers()
{
    LOG(LS_INFO) << "Changed settings detected";

    // Reload settings.
    settings_.sync();

    net::SrpUserList users = settings_.userList();

    // Merge the list of regular users and session users.
    for (const auto& session_user : ui_server_->userList())
        users.add(session_user.second);

    network_server_->setUserList(users);
}

void HostServer::startServer()
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

    ui_server_ = std::make_unique<UiServer>();

    connect(ui_server_.get(), &UiServer::processEvent, this, &HostServer::onUiProcessEvent);
    connect(ui_server_.get(), &UiServer::userListChanged, this, &HostServer::reloadUsers);
    connect(ui_server_.get(), &UiServer::killSession, this, &HostServer::stopSession);

    if (!ui_server_->start())
    {
        QCoreApplication::quit();
        return;
    }
    else
    {
        LOG(LS_INFO) << "UI server is started successfully";
    }

    network_server_ = std::make_unique<net::Server>();

    connect(network_server_.get(), &net::Server::newChannelReady,
            this, &HostServer::onNewConnection);

    if (!network_server_->start(settings_.tcpPort()))
    {
        QCoreApplication::quit();
        return;
    }
    else
    {
        LOG(LS_INFO) << "Network server is started successfully";
    }

    settings_watcher_ = std::make_unique<QFileSystemWatcher>();

    connect(settings_watcher_.get(), &QFileSystemWatcher::fileChanged,
            this, &HostServer::reloadUsers);

    settings_watcher_->addPath(settings_.filePath());

    reloadUsers();
    state_ = State::STARTED;
}

void HostServer::sendConnectEvent(const SessionProcess* session_process)
{
    if (!ui_server_)
        return;

    proto::host::ConnectEvent event;

    event.set_session_type(session_process->sessionType());
    event.set_remote_address(session_process->remoteAddress().toStdString());
    event.set_username(session_process->userName().toStdString());
    event.set_uuid(session_process->uuid());

    ui_server_->setConnectEvent(session_process->sessionId(), event);
}

} // namespace host
