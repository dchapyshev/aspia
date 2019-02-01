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

#include "host/host_server.h"

#include <QCoreApplication>

#include "base/qt_logging.h"
#include "common/message_serialization.h"
#include "host/win/host.h"
#include "host/host_settings.h"
#include "ipc/ipc_server.h"
#include "net/firewall_manager.h"
#include "net/network_channel_host.h"
#include "proto/notifier.pb.h"

namespace host {

namespace {

const char kFirewallRuleName[] = "Aspia Host Service";
const char kFirewallRuleDecription[] = "Allow incoming TCP connections";
const char kNotifierFileName[] = "aspia_host_session.exe";

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
    LOG(LS_INFO) << "Stopping the server";

    for (auto session : session_list_)
        session->stop();

    stopNotifier();

    std::unique_ptr<net::Server> network_server_deleter(network_server_);
    if (network_server_)
        network_server_->stop();

    net::FirewallManager firewall(QCoreApplication::applicationFilePath());
    if (firewall.isValid())
        firewall.deleteRuleByName(kFirewallRuleName);

    LOG(LS_INFO) << "Server is stopped";
}

void HostServer::setSessionChanged(uint32_t event, uint32_t session_id)
{
    emit sessionChanged(event, session_id);

    switch (event)
    {
        case WTS_CONSOLE_CONNECT:
        {
            if (!session_list_.isEmpty())
                startNotifier();
        }
        break;

        case WTS_CONSOLE_DISCONNECT:
        {
            has_user_session_ = false;
            stopNotifier();
        }
        break;

        case WTS_SESSION_LOGON:
        {
            if (session_id == WTSGetActiveConsoleSessionId() && !session_list_.isEmpty())
            {
                has_user_session_ = true;
                startNotifier();
            }
        }
        break;

        default:
            break;
    }
}

void HostServer::customEvent(QEvent* event)
{
    if (event->type() == kStartEvent)
    {
        LOG(LS_INFO) << "Starting the server";

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
                    LOG(LS_INFO) << "Rule is added to the firewall";
                }
            }
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

        LOG(LS_INFO) << "New connected client: " << channel->peerAddress().toStdString();

        std::unique_ptr<Host> host(new Host(this));

        host->setNetworkChannel(channel);
        host->setUuid(QUuid::createUuid());

        connect(this, &HostServer::sessionChanged, host.get(), &Host::sessionChanged);

        connect(host.get(), &Host::finished,
                this, &HostServer::onHostFinished,
                Qt::QueuedConnection);

        if (host->start())
        {
            if (notifier_state_ == NotifierState::STOPPED)
                startNotifier();
            else
                sessionToNotifier(*host);

            session_list_.push_back(host.release());
        }
    }
}

void HostServer::onHostFinished(Host* host)
{
    LOG(LS_INFO) << sessionTypeToString(host->sessionType())
                 << " session is finished for " << host->userName();

    for (auto it = session_list_.begin(); it != session_list_.end(); ++it)
    {
        if (*it != host)
            continue;

        session_list_.erase(it);

        std::unique_ptr<Host> host_deleter(host);
        sessionCloseToNotifier(*host);
        break;
    }
}

void HostServer::onIpcServerStarted(const QString& channel_id)
{
    DCHECK_EQ(notifier_state_, NotifierState::STARTING);

    notifier_process_ = new HostProcess(this);

    notifier_process_->setAccount(HostProcess::User);
    notifier_process_->setSessionId(WTSGetActiveConsoleSessionId());
    notifier_process_->setProgram(
        QCoreApplication::applicationDirPath() + QLatin1Char('/') + kNotifierFileName);
    notifier_process_->setArguments(
        QStringList() << QStringLiteral("--channel_id") << channel_id);

    connect(notifier_process_, &HostProcess::errorOccurred,
            this, &HostServer::onNotifierProcessError,
            Qt::QueuedConnection);

    connect(notifier_process_, &HostProcess::finished,
            this, &HostServer::restartNotifier,
            Qt::QueuedConnection);

    // Start the process. After the start, the process must connect to the IPC server and
    // slot |onIpcNewConnection| will be called.
    notifier_process_->start();
}

void HostServer::onIpcNewConnection(ipc::Channel* channel)
{
    DCHECK_EQ(notifier_state_, NotifierState::STARTING);

    LOG(LS_INFO) << "Notifier is started";
    notifier_state_ = NotifierState::STARTED;

    // Clients can disconnect while the notifier is started.
    if (session_list_.isEmpty())
    {
        std::unique_ptr<ipc::Channel> channel_deleter(channel);
        stopNotifier();
        return;
    }

    ipc_channel_ = channel;
    ipc_channel_->setParent(this);

    connect(ipc_channel_, &ipc::Channel::disconnected,
            ipc_channel_, &ipc::Channel::deleteLater);

    connect(ipc_channel_, &ipc::Channel::disconnected,
            this, &HostServer::restartNotifier,
            Qt::QueuedConnection);

    connect(ipc_channel_, &ipc::Channel::messageReceived,
            this, &HostServer::onIpcMessageReceived);

    // Send information about all connected sessions to the notifier.
    for (const auto& session : session_list_)
        sessionToNotifier(*session);

    ipc_channel_->start();
}

void HostServer::onNotifierProcessError(HostProcess::ErrorCode error_code)
{
    if (error_code == HostProcess::NoLoggedOnUser)
    {
        LOG(LS_INFO) << "There is no logged on user. The notifier will not be started.";
        has_user_session_ = false;
        stopNotifier();
    }
    else
    {
        LOG(LS_WARNING) << "Unable to start notifier. The server will be stopped.";
        stop();
    }
}

void HostServer::restartNotifier()
{
    if (notifier_state_ == NotifierState::STOPPED)
        return;

    stopNotifier();

    // The notifier is not needed if there are no active sessions.
    if (session_list_.isEmpty() || !has_user_session_)
        return;

    startNotifier();
}

void HostServer::onIpcMessageReceived(const QByteArray& buffer)
{
    proto::notifier::NotifierToService message;

    if (!common::parseMessage(buffer, message))
    {
        LOG(LS_WARNING) << "Invaliid message from notifier.";
        stop();
        return;
    }

    if (message.has_kill_session())
    {
        LOG(LS_INFO) << "Command to terminate the session from the notifier is received.";

        QUuid uuid = QUuid::fromString(QString::fromStdString(message.kill_session().uuid()));

        for (const auto& session : session_list_)
        {
            if (session->uuid() == uuid)
            {
                session->stop();
                break;
            }
        }
    }
    else
    {
        LOG(LS_WARNING) << "Unhandled message from notifier";
    }
}

void HostServer::startNotifier()
{
    if (notifier_state_ != NotifierState::STOPPED)
        return;

    LOG(LS_INFO) << "Starting the notifier";
    notifier_state_ = NotifierState::STARTING;

    ipc::Server* ipc_server = new ipc::Server(this);

    connect(ipc_server, &ipc::Server::started, this, &HostServer::onIpcServerStarted);
    connect(ipc_server, &ipc::Server::finished, ipc_server, &ipc::Server::deleteLater);
    connect(ipc_server, &ipc::Server::newConnection, this, &HostServer::onIpcNewConnection);
    connect(ipc_server, &ipc::Server::errorOccurred, this, &HostServer::stop, Qt::QueuedConnection);

    // Start IPC server. After its successful start, slot |onIpcServerStarted| will be called,
    // which will start the process.
    ipc_server->start();
}

void HostServer::stopNotifier()
{
    if (notifier_state_ == NotifierState::STOPPED)
        return;

    notifier_state_ = NotifierState::STOPPED;

    if (!ipc_channel_.isNull())
        ipc_channel_->stop();

    if (!notifier_process_.isNull())
    {
        notifier_process_->terminate();
        delete notifier_process_;
    }

    LOG(LS_INFO) << "Notifier is stopped";
}

void HostServer::sessionToNotifier(const Host& host)
{
    if (ipc_channel_.isNull())
        return;

    proto::notifier::ServiceToNotifier message;

    proto::notifier::Session* session = message.mutable_session();
    session->set_uuid(host.uuid().toString().toStdString());
    session->set_remote_address(host.remoteAddress().toStdString());
    session->set_username(host.userName().toStdString());
    session->set_session_type(host.sessionType());

    ipc_channel_->send(common::serializeMessage(message));
}

void HostServer::sessionCloseToNotifier(const Host& host)
{
    if (ipc_channel_.isNull())
        return;

    proto::notifier::ServiceToNotifier message;
    message.mutable_session_close()->set_uuid(host.uuid().toString().toStdString());
    ipc_channel_->send(common::serializeMessage(message));
}

} // namespace host
