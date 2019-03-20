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
#include "base/win/session_info.h"
#include "host/win/host_session_process.h"
#include "host/host_settings.h"
#include "net/firewall_manager.h"
#include "net/network_channel_host.h"

#include <QCoreApplication>
#include <QFileSystemWatcher>

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
    if (state_ == State::STOPPED || state_ == State::STOPPING)
        return;

    LOG(LS_INFO) << "Stopping the server.";
    state_ = State::STOPPING;

    delete settings_watcher_;

    for (auto session : sessions_)
    {
        session->stop();
        delete session;
    }

    sessions_.clear();

    if (ui_server_)
    {
        ui_server_->stop();
        delete ui_server_;
    }

    if (network_server_)
    {
        network_server_->stop();
        delete network_server_;
    }

    net::FirewallManager firewall(QCoreApplication::applicationFilePath());
    if (firewall.isValid())
        firewall.deleteRuleByName(kFirewallRuleName);

    LOG(LS_INFO) << "Server is stopped";
    state_ = State::STOPPED;
}

void HostServer::setSessionEvent(base::win::SessionStatus status, base::win::SessionId session_id)
{
    for (const auto session : sessions_)
        session->setSessionEvent(status, session_id);

    if (ui_server_)
        ui_server_->setSessionEvent(status, session_id);

    if (status == base::win::SessionStatus::SESSION_LOGOFF)
    {
        for (auto it = dettach_timers_.begin(); it != dettach_timers_.end();)
        {
            if (it->first == session_id)
            {
                killTimer(it->second);
                it = dettach_timers_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
}

void HostServer::stopSession(const std::string& uuid)
{
    for (auto session : sessions_)
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
    {
        LOG(LS_INFO) << "Starting the server";

        if (network_server_)
        {
            DLOG(LS_WARNING) << "An attempt was start an already running server";
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
                    LOG(LS_INFO) << "Rule is added to the firewall";
                }
            }
        }

        ui_server_ = new UiServer(this);

        connect(ui_server_, &UiServer::processEvent, this, &HostServer::onUiProcessEvent);
        connect(ui_server_, &UiServer::killSession, this, &HostServer::stopSession);

        if (!ui_server_->start())
        {
            QCoreApplication::quit();
            return;
        }
        else
        {
            LOG(LS_INFO) << "UI server is started successfully";
        }

        network_server_ = new net::Server(this);
        network_server_->setUserList(settings.userList());

        connect(network_server_, &net::Server::newChannelReady,
                this, &HostServer::onNewConnection);

        if (!network_server_->start(settings.tcpPort()))
        {
            QCoreApplication::quit();
            return;
        }
        else
        {
            LOG(LS_INFO) << "Network server is started successfully";
        }

        settings_watcher_ = new QFileSystemWatcher(this);

        connect(settings_watcher_, &QFileSystemWatcher::fileChanged,
                this, &HostServer::onSettingsChanged);

        settings_watcher_->addPath(settings.filePath());

        state_ = State::STARTED;
    }

    QObject::customEvent(event);
}

void HostServer::timerEvent(QTimerEvent* event)
{
    for (auto it = dettach_timers_.begin(); it != dettach_timers_.end();)
    {
        if (it->second == event->timerId())
        {
            killTimer(event->timerId());

            for (auto session : sessions_)
            {
                if (session->sessionId() == it->first)
                    session->stop();
            }

            it = dettach_timers_.erase(it);
        }
        else
        {
            ++it;
        }
    }

    QObject::timerEvent(event);
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

        base::win::SessionId session_id = base::win::activeConsoleSessionId();
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

        std::unique_ptr<SessionProcess> session_process(new SessionProcess(this));

        session_process->setNetworkChannel(channel);
        session_process->setUuid(base::Guid::create().toStdString());

        connect(session_process.get(), &SessionProcess::finished,
                this, &HostServer::onSessionFinished,
                Qt::QueuedConnection);

        if (session_process->start(session_id))
        {
            SessionProcess* released_session_process = session_process.release();
            sessions_.push_back(released_session_process);
            sendConnectEvent(released_session_process);
        }
    }
}

void HostServer::onUiProcessEvent(UiServer::EventType event, base::win::SessionId session_id)
{
    for (auto session : sessions_)
    {
        if (session->sessionId() != session_id)
            continue;

        if (event == UiServer::EventType::CONNECTED)
        {
            sendConnectEvent(session);

            auto result = dettach_timers_.find(session_id);
            if (result != dettach_timers_.end())
            {
                killTimer(result->second);
                dettach_timers_.erase(result);

                session->attachSession(session_id);
            }
        }
        else
        {
            DCHECK(event == UiServer::EventType::DISCONNECTED);

            session->dettachSession();

            auto result = dettach_timers_.find(session_id);
            if (result == dettach_timers_.end())
            {
                int timer_id = startTimer(std::chrono::seconds(60));
                if (!timer_id)
                {
                    LOG(LS_ERROR) << "startTimer failed";
                    session->stop();
                }
                else
                {
                    dettach_timers_.emplace(session_id, timer_id);
                }
            }
        }
    }
}

void HostServer::onSessionFinished()
{
    if (state_ == State::STOPPING)
        return;

    for (auto it = sessions_.begin(); it != sessions_.end();)
    {
        SessionProcess* session_process = *it;

        if (session_process->state() == SessionProcess::State::STOPPED)
        {
            LOG(LS_INFO) << sessionTypeToString(session_process->sessionType())
                         << " session is finished for " << session_process->userName();

            if (ui_server_)
            {
                ui_server_->setDisconnectEvent(session_process->sessionId(),
                                               session_process->uuid());
            }

            it = sessions_.erase(it);
            delete session_process;
        }
        else
        {
            ++it;
        }
    }
}

void HostServer::onSettingsChanged()
{
    Settings settings;
    network_server_->setUserList(settings.userList());
}

void HostServer::sendConnectEvent(const SessionProcess* session_process)
{
    if (!ui_server_)
        return;

    proto::host::ConnectEvent event;

    event.set_session_type(session_process->sessionType());
    event.set_remote_address(session_process->remoteAddress().toStdString());
    event.set_username(session_process->userName());
    event.set_uuid(session_process->uuid());

    ui_server_->setConnectEvent(session_process->sessionId(), event);
}

} // namespace host
