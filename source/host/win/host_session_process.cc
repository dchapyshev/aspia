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

#include "host/win/host_session_process.h"

#include "host/host_session_fake.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_server.h"
#include "net/network_channel_host.h"
#include "qt_base/qt_logging.h"

#include <QCoreApplication>

namespace host {

SessionProcess::SessionProcess(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

SessionProcess::~SessionProcess()
{
    stop();
}

void SessionProcess::setNetworkChannel(net::ChannelHost* network_channel)
{
    if (state_ != State::STOPPED)
    {
        DLOG(LS_ERROR) << "An attempt to set a network channel in an already running session process";
        return;
    }

    if (!network_channel)
    {
        DLOG(LS_ERROR) << "Network channel is null";
        return;
    }

    network_channel_ = network_channel;
    network_channel_->setParent(this);
}

void SessionProcess::setUuid(const std::string& uuid)
{
    if (state_ != State::STOPPED)
    {
        DLOG(LS_ERROR) << "An attempt to set a UUID in an already running session process";
        return;
    }

    uuid_ = uuid;
}

void SessionProcess::setUuid(std::string&& uuid)
{
    if (state_ != State::STOPPED)
    {
        DLOG(LS_ERROR) << "An attempt to set a UUID in an already running session process";
        return;
    }

    uuid_ = std::move(uuid);
}

const QString& SessionProcess::userName() const
{
    return network_channel_->userName();
}

proto::SessionType SessionProcess::sessionType() const
{
    return network_channel_->sessionType();
}

QString SessionProcess::remoteAddress() const
{
    return network_channel_->peerAddress();
}

bool SessionProcess::start(base::win::SessionId session_id)
{
    if (!network_channel_)
    {
        DLOG(LS_ERROR) << "Invalid network channel";
        return false;
    }

    switch (network_channel_->sessionType())
    {
        case proto::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::SESSION_TYPE_DESKTOP_VIEW:
        case proto::SESSION_TYPE_FILE_TRANSFER:
            break;

        default:
        {
            DLOG(LS_ERROR) << "Invalid session type: " << network_channel_->sessionType();
            return false;
        }
    }

    if (network_channel_->userName().isEmpty())
    {
        DLOG(LS_ERROR) << "Invalid user name";
        return false;
    }

    if (uuid_.empty())
    {
        DLOG(LS_ERROR) << "Invalid session UUID";
        return false;
    }

    if (state_ != State::STOPPED)
    {
        DLOG(LS_ERROR) << "Attempt to start a session process already running";
        return false;
    }

    LOG(LS_INFO) << "Starting the session process";
    state_ = State::STARTING;

    connect(network_channel_, &net::Channel::disconnected, this, &SessionProcess::stop);

    attach_timer_id_ = startTimer(std::chrono::minutes(1));
    if (!attach_timer_id_)
    {
        LOG(LS_ERROR) << "Could not start the timer";
        return false;
    }

    attachSession(session_id);
    return true;
}

void SessionProcess::stop()
{
    if (state_ == State::STOPPED || state_ == State::STOPPING)
        return;

    LOG(LS_INFO) << "Stopping session process";
    state_ = State::STOPPING;

    if (network_channel_->channelState() != net::Channel::ChannelState::NOT_CONNECTED)
        network_channel_->stop();

    dettachSession();

    if (attach_timer_id_)
    {
        killTimer(attach_timer_id_);
        attach_timer_id_ = 0;
    }

    state_ = State::STOPPED;

    LOG(LS_INFO) << "Session process is stopped";
    emit finished();
}

void SessionProcess::setSessionEvent(
    base::win::SessionStatus status, base::win::SessionId session_id)
{
    LOG(LS_INFO) << "Session change event " << base::win::sessionStatusToString(status)
                 << " for session " << session_id;

    if (state_ != State::ATTACHED && state_ != State::DETACHED)
        return;

    switch (status)
    {
        case base::win::SessionStatus::CONSOLE_CONNECT:
            attachSession(session_id);
            break;

        case base::win::SessionStatus::CONSOLE_DISCONNECT:
            dettachSession();
            break;

        default:
            break;
    }
}

void SessionProcess::attachSession(base::win::SessionId session_id)
{
    LOG(LS_INFO) << "Starting session process attachment to session " << session_id;

    state_ = State::STARTING;
    session_id_ = session_id;

    ipc_server_ = new ipc::Server(this);

    connect(ipc_server_, &ipc::Server::newConnection,
            this, &SessionProcess::ipcNewConnection,
            Qt::QueuedConnection);

    LOG(LS_INFO) << "Starting the IPC server";
    if (!ipc_server_->start())
    {
        stop();
        return;
    }

    LOG(LS_INFO) << "IPC server is running with channel id: " << ipc_server_->channelId();

    DCHECK(!session_process_);
    session_process_ = new HostProcess(this);

    session_process_->setSessionId(session_id_);
    session_process_->setProgram(
        QCoreApplication::applicationDirPath() + QStringLiteral("/aspia_host_session.exe"));

    QStringList arguments;

    arguments << QStringLiteral("--channel_id") << ipc_server_->channelId();
    arguments << QStringLiteral("--session_type");

    switch (network_channel_->sessionType())
    {
        case proto::SESSION_TYPE_DESKTOP_MANAGE:
            session_process_->setAccount(HostProcess::Account::System);
            arguments << QStringLiteral("desktop_manage");
            break;

        case proto::SESSION_TYPE_DESKTOP_VIEW:
            session_process_->setAccount(HostProcess::Account::System);
            arguments << QStringLiteral("desktop_view");
            break;

        case proto::SESSION_TYPE_FILE_TRANSFER:
            session_process_->setAccount(HostProcess::Account::User);
            arguments << QStringLiteral("file_transfer");
            break;

        default:
            LOG(LS_FATAL) << "Unknown session type: " << network_channel_->sessionType();
            break;
    }

    session_process_->setArguments(arguments);

    connect(session_process_, &HostProcess::finished, this, &SessionProcess::dettachSession);

    LOG(LS_INFO) << "Starting the session process";

    HostProcess::ErrorCode error_code = session_process_->start();
    if (error_code != HostProcess::NoError)
    {
        if (network_channel_->sessionType() == proto::SESSION_TYPE_FILE_TRANSFER &&
            error_code == HostProcess::NoLoggedOnUser)
        {
            if (!startFakeSession())
                stop();
        }
        else
        {
            stop();
        }
    }
}

void SessionProcess::dettachSession()
{
    if (state_ == State::STOPPED || state_ == State::DETACHED)
        return;

    if (state_ != State::STOPPING)
        state_ = State::DETACHED;

    if (ipc_channel_)
        ipc_channel_->stop();

    if (session_process_)
    {
        session_process_->terminate();
        delete session_process_;
    }

    LOG(LS_INFO) << "Session process is detached";

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
            LOG(LS_ERROR) << "Could not start the timer";
            stop();
        }
    }
}

void SessionProcess::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == attach_timer_id_)
    {
        LOG(LS_ERROR) << "Timeout of session attachment";
        stop();
    }
}

void SessionProcess::ipcNewConnection(ipc::Channel* channel)
{
    DCHECK(channel);
    DCHECK(attach_timer_id_);

    LOG(LS_INFO) << "IPC channel connected ("
                 << "PID: " << channel->clientProcessId() << ", "
                 << "SID: " << channel->clientSessionId() << ")";

    killTimer(attach_timer_id_);
    attach_timer_id_ = 0;

    delete fake_session_;

    ipc_channel_ = channel;
    ipc_channel_->setParent(this);

    delete ipc_server_;

    connect(ipc_channel_, &ipc::Channel::disconnected,
            this, &SessionProcess::dettachSession,
            Qt::QueuedConnection);

    connect(ipc_channel_, &ipc::Channel::disconnected, ipc_channel_, &ipc::Channel::deleteLater);
    connect(ipc_channel_, &ipc::Channel::messageReceived, network_channel_, &net::Channel::send);
    connect(network_channel_, &net::Channel::messageReceived, ipc_channel_, &ipc::Channel::send);

    LOG(LS_INFO) << "Session process is attached (SID: " << session_id_ << ")";
    state_ = State::ATTACHED;

    if (!network_channel_->isStarted())
        network_channel_->start();

    ipc_channel_->start();
}

bool SessionProcess::startFakeSession()
{
    LOG(LS_INFO) << "Starting a fake session";

    fake_session_ = SessionFake::create(network_channel_->sessionType(), this);
    if (!fake_session_)
    {
        LOG(LS_INFO) << "Session type " << network_channel_->sessionType()
                     << " does not have support for fake sessions";
        return false;
    }

    connect(fake_session_, &SessionFake::sendMessage, network_channel_, &net::Channel::send);

    connect(network_channel_, &net::Channel::messageReceived,
            fake_session_, &SessionFake::onMessageReceived);

    connect(fake_session_, &SessionFake::errorOccurred,
            this, &SessionProcess::stop,
            Qt::QueuedConnection);

    fake_session_->startSession();
    return true;
}

} // namespace host
