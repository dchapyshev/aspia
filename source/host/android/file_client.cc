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

#include "host/android/file_client.h"

#include "base/logging.h"
#include "base/serialization.h"
#include "common/file_request_handler.h"
#include "proto/file_transfer.h"
#include "proto/peer.h"

//--------------------------------------------------------------------------------------------------
FileClient::FileClient(TcpChannel* tcp_channel, QObject* parent)
    : Client(tcp_channel, parent),
      handler_(new FileRequestHandler(this))
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
FileClient::~FileClient()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void FileClient::onStart()
{
    LOG(INFO) << "File transfer session started";
    emit sig_started();
}

//--------------------------------------------------------------------------------------------------
void FileClient::onMessage(quint8 channel_id, const QByteArray& buffer)
{
    if (channel_id != proto::peer::CHANNEL_ID_0)
    {
        LOG(INFO) << "Unhandled channel:" << channel_id;
        return;
    }

    proto::file_transfer::Request request;
    if (!parse(buffer, &request))
    {
        LOG(ERROR) << "Unable to parse file transfer request";
        return;
    }

    proto::file_transfer::Reply reply;
    handler_->doRequest(request, &reply);
    send(proto::peer::CHANNEL_ID_0, serialize(reply), true);
}
