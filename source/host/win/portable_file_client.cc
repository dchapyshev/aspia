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

#include "host/win/portable_file_client.h"

#include "base/logging.h"
#include "common/file_request_handler.h"
#include "proto/peer.h"

//--------------------------------------------------------------------------------------------------
PortableFileClient::PortableFileClient(TcpChannel* tcp_channel, QObject* parent)
    : Client(tcp_channel, parent),
      handler_(new FileRequestHandler(this))
{
    CLOG(TRACE) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
PortableFileClient::~PortableFileClient()
{
    CLOG(TRACE) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void PortableFileClient::onStart()
{
    emit sig_started();
}

//--------------------------------------------------------------------------------------------------
void PortableFileClient::onMessage(quint8 channel_id, const QByteArray& buffer)
{
    if (channel_id != proto::peer::CHANNEL_ID_0)
    {
        CLOG(WARNING) << "Unhandled channel:" << channel_id;
        return;
    }

    if (!request_.parse<proto::file_transfer::Request>(buffer))
    {
        CLOG(ERROR) << "Unable to parse file transfer request";
        return;
    }

    handler_->doRequest(request_.message<proto::file_transfer::Request>(),
                        &reply_.newMessage<proto::file_transfer::Reply>());
    send(proto::peer::CHANNEL_ID_0, reply_.serialize<proto::file_transfer::Reply>());
}
