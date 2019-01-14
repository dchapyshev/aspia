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

#include "host/host_notifier.h"

#include "common/message_serialization.h"

namespace aspia {

HostNotifier::HostNotifier(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

bool HostNotifier::start(const QString& channel_id)
{
    if (channel_id.isEmpty())
    {
        LOG(LS_WARNING) << "Invalid IPC channel id";
        return false;
    }

    ipc_channel_ = ipc::Channel::createClient(this);

    connect(ipc_channel_, &ipc::Channel::connected, ipc_channel_, &ipc::Channel::start);
    connect(ipc_channel_, &ipc::Channel::disconnected, this, &HostNotifier::stop);
    connect(ipc_channel_, &ipc::Channel::errorOccurred, this, &HostNotifier::stop);
    connect(ipc_channel_, &ipc::Channel::messageReceived, this, &HostNotifier::onIpcMessageReceived);

    ipc_channel_->connectToServer(channel_id);
    return true;
}

void HostNotifier::stop()
{
    ipc_channel_->stop();
    emit finished();
}

void HostNotifier::killSession(const std::string& uuid)
{
    proto::notifier::NotifierToService message;
    message.mutable_kill_session()->set_uuid(uuid);
    ipc_channel_->send(common::serializeMessage(message));
}

void HostNotifier::onIpcMessageReceived(const QByteArray& buffer)
{
    proto::notifier::ServiceToNotifier message;
    if (!common::parseMessage(buffer, message))
    {
        LOG(LS_WARNING) << "Invalid message from service";
        stop();
        return;
    }

    if (message.has_session())
    {
        emit sessionOpen(message.session());
    }
    else if (message.has_session_close())
    {
        emit sessionClose(message.session_close());
    }
    else
    {
        LOG(LS_WARNING) << "Unhandled message from service";
    }
}

} // namespace aspia
