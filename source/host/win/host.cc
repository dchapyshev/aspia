//
// PROJECT:         Aspia
// FILE:            host/win/host.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/win/host.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <QCoreApplication>

#include "host/win/host_process.h"
#include "host/host_session_fake.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_server.h"
#include "network/network_channel.h"

namespace aspia {

namespace {

enum MessageId { IpcMessageId, NetworkMessageId };

} // namespace

Host::Host(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

Host::~Host()
{
    stop();
}

void Host::setNetworkChannel(NetworkChannel* network_channel)
{
    if (state_ != StoppedState)
    {
        qWarning("An attempt to set a network channel in an already running host.");
        return;
    }

    if (!network_channel)
    {
        qWarning("Network channel is null");
        return;
    }

    network_channel_ = network_channel;
    network_channel_->setParent(this);
}

void Host::setSessionType(proto::auth::SessionType session_type)
{
    if (state_ != StoppedState)
    {
        qWarning("An attempt to set a session type in an already running host.");
        return;
    }

    session_type_ = session_type;
}

void Host::setUserName(const QString& user_name)
{
    if (state_ != StoppedState)
    {
        qWarning("An attempt to set a user name in an already running host.");
        return;
    }

    user_name_ = user_name;
}

void Host::setUuid(const QString& uuid)
{
    if (state_ != StoppedState)
    {
        qWarning("An attempt to set a UUID in an already running host.");
        return;
    }

    uuid_ = uuid;
}

QString Host::remoteAddress() const
{
    return network_channel_->peerAddress();
}

bool Host::start()
{
    if (network_channel_.isNull())
    {
        qWarning("Invalid network channel");
        return false;
    }

    switch (session_type_)
    {
        case proto::auth::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::auth::SESSION_TYPE_DESKTOP_VIEW:
        case proto::auth::SESSION_TYPE_FILE_TRANSFER:
        case proto::auth::SESSION_TYPE_SYSTEM_INFO:
            break;

        default:
        {
            qWarning("Invalid session type: %d", session_type_);
            return false;
        }
    }

    if (user_name_.isEmpty())
    {
        qWarning("Invalid user name");
        return false;
    }

    if (uuid_.isEmpty())
    {
        qWarning("Invalid session UUID");
        return false;
    }

    if (state_ != StoppedState)
    {
        qWarning("Attempt to start a host already running");
        return false;
    }

    qInfo("Starting the host");
    state_ = StartingState;

    connect(network_channel_, &NetworkChannel::disconnected, this, &Host::stop);

    attach_timer_id_ = startTimer(std::chrono::minutes(1));
    if (!attach_timer_id_)
    {
        qWarning("Could not start the timer");
        return false;
    }

    attachSession(WTSGetActiveConsoleSessionId());
    return true;
}

void Host::stop()
{
    if (state_ == StoppedState || state_ == StoppingState)
        return;

    qInfo("Stopping host");
    state_ = StoppingState;

    if (network_channel_->channelState() != NetworkChannel::NotConnected)
        network_channel_->stop();

    dettachSession();

    if (attach_timer_id_)
    {
        killTimer(attach_timer_id_);
        attach_timer_id_ = 0;
    }

    state_ = StoppedState;

    qInfo("Host is stopped");
    emit finished(this);
}

void Host::sessionChanged(quint32 event, quint32 session_id)
{
    qInfo() << "Session change event" << event << "for session" << session_id;

    if (state_ != AttachedState && state_ != DetachedState)
        return;

    switch (event)
    {
        case WTS_CONSOLE_CONNECT:
            attachSession(session_id);
            break;

        case WTS_CONSOLE_DISCONNECT:
            dettachSession();
            break;

        default:
            break;
    }
}

void Host::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == attach_timer_id_)
    {
        qWarning("Timeout of session attachment");
        stop();
    }
}

void Host::networkMessageWritten(int message_id)
{
    Q_ASSERT(message_id == NetworkMessageId);

    if (!ipc_channel_.isNull())
        ipc_channel_->readMessage();
}

void Host::networkMessageReceived(const QByteArray& buffer)
{
    if (!ipc_channel_.isNull())
        ipc_channel_->writeMessage(IpcMessageId, buffer);
}

void Host::ipcMessageWritten(int message_id)
{
    Q_ASSERT(message_id == IpcMessageId);
    network_channel_->readMessage();
}

void Host::ipcMessageReceived(const QByteArray& buffer)
{
    network_channel_->writeMessage(NetworkMessageId, buffer);
}

void Host::ipcServerStarted(const QString& channel_id)
{
    Q_ASSERT(state_ == StartingState);
    Q_ASSERT(session_process_.isNull());

    session_process_ = new HostProcess(this);

    session_process_->setSessionId(session_id_);
    session_process_->setProgram(
        QCoreApplication::applicationDirPath() + QLatin1String("/aspia_host.exe"));

    QStringList arguments;

    arguments << QStringLiteral("--channel_id") << channel_id;
    arguments << QStringLiteral("--session_type");

    switch (session_type_)
    {
        case proto::auth::SESSION_TYPE_DESKTOP_MANAGE:
            session_process_->setAccount(HostProcess::Account::System);
            arguments << QStringLiteral("desktop_manage");
            break;

        case proto::auth::SESSION_TYPE_DESKTOP_VIEW:
            session_process_->setAccount(HostProcess::Account::System);
            arguments << QStringLiteral("desktop_view");
            break;

        case proto::auth::SESSION_TYPE_FILE_TRANSFER:
            session_process_->setAccount(HostProcess::Account::User);
            arguments << QStringLiteral("file_transfer");
            break;

        case proto::auth::SESSION_TYPE_SYSTEM_INFO:
            session_process_->setAccount(HostProcess::Account::System);
            arguments << QStringLiteral("system_info");
            break;

        default:
            qFatal("Unknown session type: %d", session_type_);
            break;
    }

    session_process_->setArguments(arguments);

    connect(session_process_, &HostProcess::errorOccurred, [this](HostProcess::ErrorCode error_code)
    {
        if (session_type_ == proto::auth::SESSION_TYPE_FILE_TRANSFER &&
            error_code == HostProcess::NoLoggedOnUser)
        {
            if (!startFakeSession())
                stop();
        }
        else
        {
            stop();
        }
    });

    connect(session_process_, &HostProcess::finished, this, &Host::dettachSession);

    session_process_->start();
}

void Host::ipcNewConnection(IpcChannel* channel)
{
    Q_ASSERT(channel);
    Q_ASSERT(attach_timer_id_);

    killTimer(attach_timer_id_);
    attach_timer_id_ = 0;

    delete fake_session_;

    ipc_channel_ = channel;
    ipc_channel_->setParent(this);

    connect(ipc_channel_, &IpcChannel::disconnected, ipc_channel_, &IpcChannel::deleteLater);
    connect(ipc_channel_, &IpcChannel::disconnected, this, &Host::dettachSession);
    connect(ipc_channel_, &IpcChannel::messageReceived, this, &Host::ipcMessageReceived);
    connect(ipc_channel_, &IpcChannel::messageWritten, this, &Host::ipcMessageWritten);

    connect(network_channel_, &NetworkChannel::messageWritten, this, &Host::networkMessageWritten);
    connect(network_channel_, &NetworkChannel::messageReceived, this, &Host::networkMessageReceived);

    qInfo() << "Host process is attached for session" << session_id_;
    state_ = AttachedState;

    ipc_channel_->readMessage();
    network_channel_->readMessage();
}

void Host::attachSession(quint32 session_id)
{
    qInfo() << "Starting host process attachment to session" << session_id;

    state_ = StartingState;
    session_id_ = session_id;

    IpcServer* ipc_server = new IpcServer(this);

    connect(ipc_server, &IpcServer::started, this, &Host::ipcServerStarted);
    connect(ipc_server, &IpcServer::finished, ipc_server, &IpcServer::deleteLater);
    connect(ipc_server, &IpcServer::newConnection, this, &Host::ipcNewConnection);
    connect(ipc_server, &IpcServer::errorOccurred, this, &Host::stop);

    ipc_server->start();
}

void Host::dettachSession()
{
    if (state_ == StoppedState || state_ == DetachedState)
        return;

    if (state_ != StoppingState)
        state_ = DetachedState;

    disconnect(network_channel_, &NetworkChannel::messageWritten,
               this, &Host::networkMessageWritten);
    disconnect(network_channel_, &NetworkChannel::messageReceived,
               this, &Host::networkMessageReceived);

    if (!ipc_channel_.isNull() && ipc_channel_->channelState() == IpcChannel::Connected)
        ipc_channel_->stop();

    if (!session_process_.isNull())
    {
        session_process_->kill();
        delete session_process_;
    }

    qInfo("Host process is detached");

    if (state_ == StoppingState)
        return;

    if (!startFakeSession())
    {
        stop();
    }
    else
    {
        attach_timer_id_ = startTimer(std::chrono::minutes(1));
        if (!attach_timer_id_)
        {
            qWarning("Could not start the timer");
            stop();
        }
    }
}

bool Host::startFakeSession()
{
    qInfo("Starting a fake session");

    fake_session_ = HostSessionFake::create(session_type_, this);
    if (fake_session_.isNull())
    {
        qInfo() << "Session type" << session_type_ << "does not have support for fake sessions";
        return false;
    }

    connect(fake_session_, &HostSessionFake::writeMessage,
            network_channel_, &NetworkChannel::writeMessage);

    connect(fake_session_, &HostSessionFake::readMessage,
            network_channel_, &NetworkChannel::readMessage);

    connect(network_channel_, &NetworkChannel::messageReceived,
            fake_session_, &HostSessionFake::onMessageReceived);

    connect(network_channel_, &NetworkChannel::messageWritten,
            fake_session_, &HostSessionFake::onMessageWritten);

    connect(fake_session_, &HostSessionFake::errorOccurred, this, &Host::stop);

    fake_session_->startSession();
    return true;
}

} // namespace aspia
