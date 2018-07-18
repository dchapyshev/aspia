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
#include <QDebug>
#include <QUuid>

#include "base/message_serialization.h"
#include "host/win/host.h"
#include "host/host_user_authorizer.h"
#include "ipc/ipc_server.h"
#include "network/firewall_manager.h"
#include "network/network_channel.h"
#include "protocol/notifier.pb.h"

namespace aspia {

namespace {

const char kFirewallRuleName[] = "Aspia Host Service";
const char kNotifierFileName[] = "aspia_host_notifier.exe";

const char* sessionTypeToString(proto::auth::SessionType session_type)
{
    switch (session_type)
    {
        case proto::auth::SESSION_TYPE_DESKTOP_MANAGE:
            return "Desktop Manage";

        case proto::auth::SESSION_TYPE_DESKTOP_VIEW:
            return "Desktop View";

        case proto::auth::SESSION_TYPE_FILE_TRANSFER:
            return "File Transfer";

        case proto::auth::SESSION_TYPE_SYSTEM_INFO:
            return "System Information";

        default:
            return "Unknown";
    }
}

const char* statusToString(proto::auth::Status status)
{
    switch (status)
    {
        case proto::auth::STATUS_SUCCESS:
            return "Success";

        case proto::auth::STATUS_ACCESS_DENIED:
            return "Access Denied";

        case proto::auth::STATUS_CANCELED:
            return "Canceled";

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

bool HostServer::start(int port, const QList<User>& user_list)
{
    qInfo("Starting the server");

    if (!network_server_.isNull())
    {
        qWarning("An attempt was start an already running server.");
        return false;
    }

    user_list_ = user_list;
    if (user_list_.isEmpty())
    {
        qWarning("Empty user list");
    }

    FirewallManager firewall(QCoreApplication::applicationFilePath());
    if (firewall.isValid())
    {
        if (firewall.addTcpRule(kFirewallRuleName,
                                tr("Allow incoming TCP connections"),
                                port))
        {
            qInfo("Rule is added to the firewall");
        }
    }

    network_server_ = new NetworkServer(this);

    connect(network_server_, &NetworkServer::newChannelReady,
            this, &HostServer::onNewConnection);

    if (!network_server_->start(port))
        return false;

    qInfo() << "Server is started on port" << port;
    return true;
}

void HostServer::stop()
{
    qInfo("Stopping the server");

    for (auto session : session_list_)
        session->stop();

    stopNotifier();

    if (!network_server_.isNull())
    {
        network_server_->stop();
        delete network_server_;
    }

    user_list_.clear();

    FirewallManager firewall(QCoreApplication::applicationFilePath());
    if (firewall.isValid())
        firewall.deleteRuleByName(kFirewallRuleName);

    qInfo("Server is stopped");
}

void HostServer::setSessionChanged(quint32 event, quint32 session_id)
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
            if (restart_timer_id_ != 0)
            {
                killTimer(restart_timer_id_);
                restart_timer_id_ = 0;
            }

            stopNotifier();
        }
        break;

        case WTS_SESSION_LOGON:
        {
            if (session_id == WTSGetActiveConsoleSessionId() && !session_list_.isEmpty())
                startNotifier();
        }
        break;

        default:
            break;
    }
}

void HostServer::timerEvent(QTimerEvent* event)
{
    if (restart_timer_id_ != 0 && event->timerId() == restart_timer_id_)
    {
        killTimer(restart_timer_id_);
        restart_timer_id_ = 0;

        if (session_list_.isEmpty())
            return;

        startNotifier();
        return;
    }

    QObject::timerEvent(event);
}

void HostServer::onNewConnection()
{
    while (network_server_->hasReadyChannels())
    {
        NetworkChannel* channel = network_server_->nextReadyChannel();
        if (!channel)
            continue;

        qInfo() << "New connected client:" << channel->peerAddress();

        HostUserAuthorizer* authorizer = new HostUserAuthorizer(this);

        authorizer->setNetworkChannel(channel);
        authorizer->setUserList(user_list_);

        connect(authorizer, &HostUserAuthorizer::finished,
                this, &HostServer::onAuthorizationFinished);

        qInfo("Start authorization");
        authorizer->start();
    }
}

void HostServer::onAuthorizationFinished(HostUserAuthorizer* authorizer)
{
    qInfo() << "Authorization for" << authorizer->userName()
            << "completed with status:" << statusToString(authorizer->status());

    QScopedPointer<HostUserAuthorizer> authorizer_deleter(authorizer);

    if (authorizer->status() != proto::auth::STATUS_SUCCESS)
        return;

    QScopedPointer<Host> host(new Host(this));

    host->setNetworkChannel(authorizer->networkChannel());
    host->setSessionType(authorizer->sessionType());
    host->setUserName(authorizer->userName());
    host->setUuid(QUuid::createUuid().toString());

    connect(this, &HostServer::sessionChanged, host.data(), &Host::sessionChanged);
    connect(host.data(), &Host::finished, this, &HostServer::onHostFinished, Qt::QueuedConnection);

    qInfo() << "Starting" << sessionTypeToString(authorizer->sessionType())
            << "session for" << authorizer->userName();

    if (host->start())
    {
        if (notifier_state_ == NotifierState::Stopped)
            startNotifier();
        else
            sessionToNotifier(*host);

        session_list_.push_back(host.take());
    }
}

void HostServer::onHostFinished(Host* host)
{
    qInfo() << sessionTypeToString(host->sessionType())
            << "session is finished for" << host->userName();

    for (auto it = session_list_.begin(); it != session_list_.end(); ++it)
    {
        if (*it != host)
            continue;

        session_list_.erase(it);

        QScopedPointer<Host> host_deleter(host);
        sessionCloseToNotifier(*host);
        break;
    }
}

void HostServer::onIpcServerStarted(const QString& channel_id)
{
    Q_ASSERT(notifier_state_ == NotifierState::Starting);

    notifier_process_ = new HostProcess(this);

    notifier_process_->setAccount(HostProcess::User);
    notifier_process_->setSessionId(WTSGetActiveConsoleSessionId());
    notifier_process_->setProgram(
        QCoreApplication::applicationDirPath() + QLatin1Char('/') + kNotifierFileName);
    notifier_process_->setArguments(
        QStringList() << QStringLiteral("--channel_id") << channel_id);

    connect(notifier_process_, &HostProcess::errorOccurred,
            this, &HostServer::onNotifierProcessError);

    connect(notifier_process_, &HostProcess::finished,
            this, &HostServer::restartNotifier);

    // Start the process. After the start, the process must connect to the IPC server and
    // slot |onIpcNewConnection| will be called.
    notifier_process_->start();
}

void HostServer::onIpcNewConnection(IpcChannel* channel)
{
    Q_ASSERT(notifier_state_ == NotifierState::Starting);

    qInfo("Notifier is started");
    notifier_state_ = NotifierState::Started;

    ipc_channel_ = channel;
    ipc_channel_->setParent(this);

    connect(ipc_channel_, &IpcChannel::disconnected, ipc_channel_, &IpcChannel::deleteLater);
    connect(ipc_channel_, &IpcChannel::disconnected, this, &HostServer::restartNotifier);
    connect(ipc_channel_, &IpcChannel::messageReceived, this, &HostServer::onIpcMessageReceived);

    // Send information about all connected sessions to the notifier.
    for (const auto& session : session_list_)
        sessionToNotifier(*session);

    ipc_channel_->readMessage();
}

void HostServer::onNotifierProcessError(HostProcess::ErrorCode error_code)
{
    if (error_code == HostProcess::NoLoggedOnUser)
    {
        qInfo("There is no logged on user. The notifier will not be started.");
        stopNotifier();
    }
    else
    {
        qWarning("Unable to start notifier. The server will be stopped");
        stop();
    }
}

void HostServer::restartNotifier()
{
    if (notifier_state_ == NotifierState::Stopped)
        return;

    stopNotifier();

    // The notifier is not needed if there are no active sessions.
    if (session_list_.isEmpty())
        return;

    restart_timer_id_ = startTimer(std::chrono::seconds(30));
    if (restart_timer_id_ == 0)
    {
        qWarning("Unable to start timer");
        stop();
    }
}

void HostServer::onIpcMessageReceived(const QByteArray& buffer)
{
    proto::notifier::NotifierToService message;

    if (!parseMessage(buffer, message))
    {
        qWarning("Invaliid message from notifier");
        stop();
        return;
    }

    if (message.has_kill_session())
    {
        qInfo("Command to terminate the session from the notifier is received");

        QString uuid = QString::fromStdString(message.kill_session().uuid());

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
        qWarning("Unhandled message from notifier");
    }

    // Read next message.
    ipc_channel_->readMessage();
}

void HostServer::startNotifier()
{
    if (notifier_state_ != NotifierState::Stopped)
        return;

    qInfo("Starting the notifier");
    notifier_state_ = NotifierState::Starting;

    IpcServer* ipc_server = new IpcServer(this);

    connect(ipc_server, &IpcServer::started, this, &HostServer::onIpcServerStarted);
    connect(ipc_server, &IpcServer::finished, ipc_server, &IpcServer::deleteLater);
    connect(ipc_server, &IpcServer::newConnection, this, &HostServer::onIpcNewConnection);
    connect(ipc_server, &IpcServer::errorOccurred, this, &HostServer::stop, Qt::QueuedConnection);

    // Start IPC server. After its successful start, slot |onIpcServerStarted| will be called,
    // which will start the process.
    ipc_server->start();
}

void HostServer::stopNotifier()
{
    if (notifier_state_ == NotifierState::Stopped)
        return;

    notifier_state_ = NotifierState::Stopped;

    if (!ipc_channel_.isNull() && ipc_channel_->channelState() == IpcChannel::Connected)
        ipc_channel_->stop();

    if (!notifier_process_.isNull())
    {
        notifier_process_->kill();
        delete notifier_process_;
    }

    qInfo("Notifier is stopped");
}

void HostServer::sessionToNotifier(const Host& host)
{
    if (ipc_channel_.isNull())
        return;

    proto::notifier::ServiceToNotifier message;

    proto::notifier::Session* session = message.mutable_session();
    session->set_uuid(host.uuid().toStdString());
    session->set_remote_address(host.remoteAddress().toStdString());
    session->set_username(host.userName().toStdString());
    session->set_session_type(host.sessionType());

    ipc_channel_->writeMessage(-1, serializeMessage(message));
}

void HostServer::sessionCloseToNotifier(const Host& host)
{
    if (ipc_channel_.isNull())
        return;

    proto::notifier::ServiceToNotifier message;
    message.mutable_session_close()->set_uuid(host.uuid().toStdString());
    ipc_channel_->writeMessage(-1, serializeMessage(message));
}

} // namespace aspia
