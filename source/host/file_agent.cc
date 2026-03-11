//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/file_agent.h"

#include <QCoreApplication>

#include "base/logging.h"
#include "base/serialization.h"
#include "base/ipc/ipc_channel.h"
#include "common/file_worker.h"

namespace host {

//--------------------------------------------------------------------------------------------------
FileAgent::FileAgent(QObject* parent)
    : QObject(parent),
      worker_(new common::FileWorker(this))
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
FileAgent::~FileAgent()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void FileAgent::start(const QString& channel_id)
{
    LOG(INFO) << "Starting (channel_id:" << channel_id << ")";

    ipc_channel_ = new base::IpcChannel(this);

    connect(ipc_channel_, &base::IpcChannel::sig_connected, this, &FileAgent::onIpcConnected);
    connect(ipc_channel_, &base::IpcChannel::sig_disconnected, this, &FileAgent::onIpcDisconnected);
    connect(ipc_channel_, &base::IpcChannel::sig_errorOccurred, this, &FileAgent::onIpcErrorOccurred);
    connect(ipc_channel_, &base::IpcChannel::sig_messageReceived, this, &FileAgent::onIpcMessageReceived);

    ipc_channel_->connectTo(channel_id);
}

//--------------------------------------------------------------------------------------------------
void FileAgent::onIpcConnected()
{
    ipc_channel_->setPaused(false);
}

//--------------------------------------------------------------------------------------------------
void FileAgent::onIpcDisconnected()
{
    LOG(INFO) << "IPC connection is terminated";
    QCoreApplication::quit();
}

//--------------------------------------------------------------------------------------------------
void FileAgent::onIpcErrorOccurred()
{
    LOG(INFO) << "Unable to connect to IPC server";
    QCoreApplication::quit();
}

//--------------------------------------------------------------------------------------------------
void FileAgent::onIpcMessageReceived(quint32 /* ipc_channel_id */, const QByteArray& buffer)
{
    if (!request_.parse(buffer))
    {
        LOG(ERROR) << "Unable to parse message";
        return;
    }

    worker_->doRequest(request_.message(), &reply_.newMessage());
    ipc_channel_->send(0, reply_.serialize());
}

} // namespace host
