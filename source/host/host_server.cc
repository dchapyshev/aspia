//
// PROJECT:         Aspia
// FILE:            host/host_server.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_server.h"

#include <QCoreApplication>
#include <QDebug>
#include <QUuid>

#include "base/message_serialization.h"
#include "host/win/host.h"
#include "host/host_settings.h"
#include "host/host_user_authorizer.h"
#include "ipc/ipc_server.h"
#include "network/firewall_manager.h"
#include "network/network_channel.h"
#include "protocol/notifier.pb.h"

namespace aspia {

namespace {

const char kFirewallRuleName[] = "Aspia Host Service";
const char kNotifierFileName[] = "aspia_host_notifier.exe";

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

bool HostServer::start()
{
    if (!network_server_.isNull())
    {
        qWarning("An attempt was start an already running server.");
        return false;
    }

    HostSettings settings;

    user_list_ = settings.userList();
    if (user_list_.isEmpty())
    {
        qWarning("Empty user list");
        return false;
    }

    int port = settings.tcpPort();

    FirewallManager firewall(QCoreApplication::applicationFilePath());
    if (firewall.isValid())
    {
        firewall.addTcpRule(kFirewallRuleName,
                            QCoreApplication::tr("Allow incoming TCP connections"),
                            port);
    }

    network_server_ = new NetworkServer(this);

    connect(network_server_, &NetworkServer::newChannelReady,
            this, &HostServer::onNewConnection);

    if (!network_server_->start(port))
        return false;

    return true;
}

void HostServer::stop()
{
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
}

void HostServer::setSessionChanged(quint32 event, quint32 session_id)
{
    emit sessionChanged(event, session_id);
}

void HostServer::onNewConnection()
{
    while (network_server_->hasReadyChannels())
    {
        NetworkChannel* channel = network_server_->nextReadyChannel();
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

    QScopedPointer<Host> host(new Host(this));

    host->setNetworkChannel(authorizer->networkChannel());
    host->setSessionType(authorizer->sessionType());
    host->setUserName(authorizer->userName());
    host->setUuid(QUuid::createUuid().toString());

    connect(this, &HostServer::sessionChanged, host.data(), &Host::sessionChanged);
    connect(host.data(), &Host::finished, this, &HostServer::onHostFinished, Qt::QueuedConnection);

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
    notifier_process_->setProgram(QCoreApplication::applicationDirPath() + "/" + kNotifierFileName);
    notifier_process_->setArguments(QStringList() << "--channel_id" << channel_id);

    connect(notifier_process_, &HostProcess::errorOccurred, this, &HostServer::stop);

    connect(notifier_process_, &HostProcess::finished,
            this, &HostServer::onNotifierFinished);

    // Start the process. After the start, the process must connect to the IPC server and
    // slot |onIpcNewConnection| will be called.
    notifier_process_->start();
}

void HostServer::onIpcNewConnection(IpcChannel* channel)
{
    Q_ASSERT(notifier_state_ == NotifierState::Starting);

    notifier_state_ = NotifierState::Started;

    ipc_channel_ = channel;
    ipc_channel_->setParent(this);

    connect(ipc_channel_, &IpcChannel::disconnected, ipc_channel_, &IpcChannel::deleteLater);
    connect(ipc_channel_, &IpcChannel::disconnected, this, &HostServer::onIpcDisconnected);
    connect(ipc_channel_, &IpcChannel::messageReceived, this, &HostServer::onIpcMessageReceived);

    // Send information about all connected sessions to the notifier.
    for (const auto& session : session_list_)
        sessionToNotifier(*session);

    ipc_channel_->readMessage();
}

void HostServer::onIpcDisconnected()
{
    if (notifier_state_ == NotifierState::Stopped)
        return;

    stopNotifier();

    // The notifier is not needed if there are no active sessions.
    if (session_list_.isEmpty())
        return;

    // Otherwise, restart the notifier.
    startNotifier();
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

void HostServer::onNotifierFinished()
{
    if (notifier_state_ == NotifierState::Stopped)
        return;

    stopNotifier();

    // The notifier is not needed if there are no active sessions.
    if (session_list_.isEmpty())
        return;

    // Otherwise, restart the notifier.
    startNotifier();
}

void HostServer::startNotifier()
{
    if (notifier_state_ != NotifierState::Stopped)
        return;

    notifier_state_ = NotifierState::Starting;

    IpcServer* ipc_server = new IpcServer(this);

    connect(ipc_server, &IpcServer::started, this, &HostServer::onIpcServerStarted);
    connect(ipc_server, &IpcServer::finished, ipc_server, &IpcServer::deleteLater);
    connect(ipc_server, &IpcServer::newConnection, this, &HostServer::onIpcNewConnection);
    connect(ipc_server, &IpcServer::errorOccurred, this, &HostServer::stop);

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
