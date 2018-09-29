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

#include "host/host_session_file_transfer.h"

#include "common/file_worker.h"
#include "common/message_serialization.h"

namespace aspia {

HostSessionFileTransfer::HostSessionFileTransfer(const QString& channel_id)
    : HostSession(channel_id)
{
    // Nothing
}

void HostSessionFileTransfer::startSession()
{
    worker_ = new FileWorker(this);
}

void HostSessionFileTransfer::stopSession()
{
    delete worker_;
}

void HostSessionFileTransfer::messageReceived(const QByteArray& buffer)
{
    if (worker_.isNull())
        return;

    proto::file_transfer::Request request;

    if (!parseMessage(buffer, request))
    {
        emit errorOccurred();
        return;
    }

    emit sendMessage(serializeMessage(worker_->doRequest(request)));
}

} // namespace aspia
