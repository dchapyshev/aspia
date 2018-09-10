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

#include "host/host_session.h"

#include <QCoreApplication>

#include "base/logging.h"
#include "host/host_session_desktop.h"
#include "host/host_session_file_transfer.h"
#include "ipc/ipc_channel.h"

namespace aspia {

HostSession::HostSession(const QString& channel_id)
    : channel_id_(channel_id)
{
    // Nothing
}

// static
HostSession* HostSession::create(const QString& session_type, const QString& channel_id)
{
    if (channel_id.isEmpty())
    {
        LOG(LS_WARNING) << "Invalid IPC channel id";
        return nullptr;
    }

    if (session_type == QLatin1String("desktop_manage"))
    {
        return new HostSessionDesktop(proto::SESSION_TYPE_DESKTOP_MANAGE, channel_id);
    }
    else if (session_type == QLatin1String("desktop_view"))
    {
        return new HostSessionDesktop(proto::SESSION_TYPE_DESKTOP_VIEW, channel_id);
    }
    else if (session_type == QLatin1String("file_transfer"))
    {
        return new HostSessionFileTransfer(channel_id);
    }
    else
    {
        LOG(LS_WARNING) << "Unknown session type: " << session_type.toStdString();
        return nullptr;
    }
}

void HostSession::start()
{
    ipc_channel_ = IpcChannel::createClient(this);

    connect(ipc_channel_, &IpcChannel::connected, ipc_channel_, &IpcChannel::start);
    connect(ipc_channel_, &IpcChannel::connected, this, &HostSession::startSession);
    connect(ipc_channel_, &IpcChannel::disconnected, this, &HostSession::stop);
    connect(ipc_channel_, &IpcChannel::errorOccurred, this, &HostSession::stop);
    connect(ipc_channel_, &IpcChannel::messageReceived, this, &HostSession::messageReceived);

    connect(this, &HostSession::sendMessage, ipc_channel_, &IpcChannel::send);
    connect(this, &HostSession::errorOccurred, this, &HostSession::stop);

    ipc_channel_->connectToServer(channel_id_);
}

void HostSession::stop()
{
    stopSession();
    QCoreApplication::quit();
}

} // namespace aspia
