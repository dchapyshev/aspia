//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/file_transfer_agent.h"

#include "base/logging.h"
#include "base/serialization.h"

#include <QCoreApplication>

namespace host {

//--------------------------------------------------------------------------------------------------
FileTransferAgent::FileTransferAgent(QObject* parent)
    : QObject(parent)
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
FileTransferAgent::~FileTransferAgent()
{
    LOG(LS_INFO) << "Dtor";
}

void FileTransferAgent::start(const QString& channel_id)
{
    LOG(LS_INFO) << "Starting (channel_id=" << channel_id.data() << ")";

    channel_ = std::make_unique<base::IpcChannel>();

    if (!channel_->connect(channel_id))
    {
        LOG(LS_ERROR) << "Connection failed";
        return;
    }

    channel_->setListener(this);
    channel_->resume();
}

//--------------------------------------------------------------------------------------------------
void FileTransferAgent::onIpcDisconnected()
{
    LOG(LS_INFO) << "IPC channel disconnected";
    QCoreApplication::quit();
}

//--------------------------------------------------------------------------------------------------
void FileTransferAgent::onIpcMessageReceived(const QByteArray& buffer)
{
    request_.Clear();
    reply_.Clear();

    if (!base::parse(buffer, &request_))
    {
        LOG(LS_ERROR) << "Unable to parse message";
        return;
    }

    worker_.doRequest(request_, &reply_);
    channel_->send(base::serialize(reply_));
}

//--------------------------------------------------------------------------------------------------
void FileTransferAgent::onIpcMessageWritten()
{
    // Nothing
}

} // namespace host
