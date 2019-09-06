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

#include "client/client_file_transfer.h"

#include "client/file_request_producer_proxy.h"
#include "common/file_worker.h"

namespace client {

ClientFileTransfer::ClientFileTransfer(const Config& config)
    : Client(config),
      local_worker_(std::make_unique<common::FileWorker>())
{
    // Nothing
}

ClientFileTransfer::~ClientFileTransfer() = default;

void ClientFileTransfer::onMessageReceived(const base::ByteArray& buffer)
{
    proto::FileReply reply;

    if (!reply.ParseFromArray(buffer.data(), buffer.size()))
    {
        onSessionError(tr("Invalid message from host"));
        return;
    }

    if (reply.status() == proto::FileReply::STATUS_NO_LOGGED_ON_USER)
    {
        onSessionError(tr("There are no logged in users. File transfer is not available"));
        return;
    }

    request_producer_->onRemoteReply(reply);
}

void ClientFileTransfer::localRequest(const proto::FileRequest& request)
{
    request_producer_->onLocalReply(local_worker_->doRequest(request));
}

void ClientFileTransfer::remoteRequest(const proto::FileRequest& request)
{
    sendMessage(request);
}

void ClientFileTransfer::onSessionError(const QString& message)
{
    emit errorOccurred(QString("%1: %2.").arg(tr("Session error")).arg(message));
}

} // namespace client
