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

#include "host/host_ui_process.h"

#include <QCoreApplication>
#include <QUuid>

#include "base/qt_logging.h"
#include "common/message_serialization.h"
#include "host/win/host.h"
#include "ipc/ipc_channel.h"
#include "proto/notifier.pb.h"

namespace host {

namespace {

const char kFileName[] = "aspia_host.exe";

} // namespace

UiProcess::UiProcess(ipc::Channel* channel, QObject* parent)
    : QObject(parent),
      channel_(channel)
{
    channel_->setParent(this);
}

UiProcess::~UiProcess() = default;

// static
void UiProcess::create(base::win::SessionId session_id)
{
    LOG(LS_INFO) << "Starting the UI (SID: " << session_id << ").";

    std::unique_ptr<HostProcess> process(new HostProcess());

    process->setAccount(HostProcess::User);
    process->setSessionId(session_id);
    process->setProgram(QCoreApplication::applicationDirPath() + QLatin1Char('/') + kFileName);

    process->start();
}

base::win::SessionId UiProcess::sessionId() const
{
    return channel_->clientSessionId();
}

void UiProcess::setConnectEvent(const proto::notifier::ConnectEvent& event)
{
    proto::notifier::ServiceToNotifier message;
    message.mutable_connect_event()->CopyFrom(event);
    channel_->send(common::serializeMessage(message));
}

void UiProcess::setDisconnectEvent(const std::string& uuid)
{
    proto::notifier::ServiceToNotifier message;
    message.mutable_disconnect_event()->set_uuid(uuid);
    channel_->send(common::serializeMessage(message));
}

void UiProcess::start()
{
    if (state_ != State::STOPPED)
    {
        DLOG(LS_ERROR) << "Attempting to start a process that is already running.";
        return;
    }

    connect(channel_, &ipc::Channel::messageReceived, this, &UiProcess::onMessageReceived);
    connect(channel_, &ipc::Channel::disconnected, this, &UiProcess::stop);

    state_ = State::STARTED;
    channel_->start();
}

void UiProcess::stop()
{
    if (state_ == State::STOPPED)
        return;

    state_ = State::STOPPED;

    if (channel_)
        channel_->stop();

    LOG(LS_INFO) << "UI is stopped.";
    emit finished();
}

void UiProcess::onMessageReceived(const QByteArray& buffer)
{
    proto::notifier::NotifierToService message;

    if (!common::parseMessage(buffer, message))
    {
        LOG(LS_ERROR) << "Invalid message from UI.";
        stop();
        return;
    }

    if (message.has_kill_session())
    {
        QString uuid = QString::fromStdString(message.kill_session().uuid());
        emit killSession(QUuid::fromString(uuid));
    }
    else
    {
        LOG(LS_WARNING) << "Unhandled message from UI.";
    }
}

} // namespace host
