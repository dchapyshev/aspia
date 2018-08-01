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

#include "host/win/host.h"

#include <qt_windows.h>

#include <QCoreApplication>

#include "host/win/host_process.h"
#include "host/host_session_fake.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_server.h"
#include "network/network_channel.h"

namespace aspia {

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
    if (state_ != State::STOPPED)
    {
        qDebug("An attempt to set a network channel in an already running host.");
        return;
    }

    if (!network_channel)
    {
        qDebug("Network channel is null");
        return;
    }

    network_channel_ = network_channel;
    network_channel_->setParent(this);
}

void Host::setSessionType(proto::auth::SessionType session_type)
{
    if (state_ != State::STOPPED)
    {
        qDebug("An attempt to set a session type in an already running host");
        return;
    }

    session_type_ = session_type;
}

void Host::setUserName(const QString& user_name)
{
    if (state_ != State::STOPPED)
    {
        qDebug("An attempt to set a user name in an already running host");
        return;
    }

    user_name_ = user_name;
}

void Host::setUuid(const QString& uuid)
{
    if (state_ != State::STOPPED)
    {
        qDebug("An attempt to set a UUID in an already running host");
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
        qDebug("Invalid network channel");
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
            qDebug("Invalid session type: %d", session_type_);
            return false;
        }
    }

    if (user_name_.isEmpty())
    {
        qDebug("Invalid user name");
        return false;
    }

    if (uuid_.isEmpty())
    {
        qDebug("Invalid session UUID");
        return false;
    }

    if (state_ != State::STOPPED)
    {
        qDebug("Attempt to start a host already running");
        return false;
    }

    qInfo("Starting the host");
    state_ = State::STARTING;

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
    if (state_ == State::STOPPED || state_ == State::STOPPING)
        return;

    qInfo("Stopping host");
    state_ = State::STOPPING;

    if (network_channel_->state() != NetworkChannel::State::NOT_CONNECTED)
        network_channel_->stop();

    dettachSession();

    if (attach_timer_id_)
    {
        killTimer(attach_timer_id_);
        attach_timer_id_ = 0;
    }

    state_ = State::STOPPED;

    qInfo("Host is stopped");
    emit finished(this);
}

void Host::sessionChanged(uint32_t event, uint32_t session_id)
{
    qInfo() << "Session change event" << event << "for session" << session_id;

    if (state_ != State::ATTACHED && state_ != State::DETACHED)
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

void Host::ipcServerStarted(const QString& channel_id)
{
    Q_ASSERT(state_ == State::STARTING);
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
    connect(ipc_channel_, &IpcChannel::messageReceived, network_channel_, &NetworkChannel::send);
    connect(network_channel_, &NetworkChannel::messageReceived, ipc_channel_, &IpcChannel::send);

    qInfo() << "Host process is attached for session" << session_id_;
    state_ = State::ATTACHED;

    if (!network_channel_->isStarted())
        network_channel_->start();

    ipc_channel_->start();
}

void Host::attachSession(uint32_t session_id)
{
    qInfo() << "Starting host process attachment to session" << session_id;

    state_ = State::STARTING;
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
    if (state_ == State::STOPPED || state_ == State::DETACHED)
        return;

    if (state_ != State::STOPPING)
        state_ = State::DETACHED;

    if (!ipc_channel_.isNull())
        ipc_channel_->stop();

    if (!session_process_.isNull())
    {
        session_process_->terminate();
        delete session_process_;
    }

    qInfo("Host process is detached");

    if (state_ == State::STOPPING)
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

    connect(fake_session_, &HostSessionFake::sendMessage, network_channel_, &NetworkChannel::send);

    connect(network_channel_, &NetworkChannel::messageReceived,
            fake_session_, &HostSessionFake::onMessageReceived);

    connect(fake_session_, &HostSessionFake::errorOccurred, this, &Host::stop);

    fake_session_->startSession();
    return true;
}

} // namespace aspia
