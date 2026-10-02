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

#include "base/core_application.h"
#include "base/logging.h"
#include "base/serialization.h"
#include "base/session_id.h"
#include "base/win/session_info.h"
#include "host/workers/portable_file_worker.h"
#include "proto/file_transfer.h"
#include "proto/peer.h"

//--------------------------------------------------------------------------------------------------
PortableFileClient::PortableFileClient(TcpChannel* tcp_channel, QObject* parent)
    : Client(tcp_channel, parent),
      file_worker_(CoreApplication::findWorker<PortableFileWorker>())
{
    CLOG(TRACE) << "Ctor";
    CCHECK(file_worker_);

    connect(this, &Client::sig_finished, this, [this]()
    {
        if (file_worker_)
            file_worker_->removeClient(clientId());
    });
}

//--------------------------------------------------------------------------------------------------
PortableFileClient::~PortableFileClient()
{
    CLOG(TRACE) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void PortableFileClient::onStart()
{
    SessionInfo session_info(currentProcessSessionId());
    if (!session_info.isValid())
    {
        CLOG(ERROR) << "Unable to get session info";
        finish();
        return;
    }

    // The session starts, but responds to all client requests with an error.
    is_session_locked_ = session_info.isUserLocked();
    if (is_session_locked_)
        CLOG(WARNING) << "User session is locked";

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

    if (is_session_locked_)
    {
        proto::file_transfer::Request request;
        if (!parse(buffer, &request))
        {
            CLOG(ERROR) << "Unable to parse file transfer request";
            return;
        }

        proto::file_transfer::Reply reply;
        reply.set_request_id(request.request_id());
        reply.set_error_code(proto::file_transfer::ERROR_CODE_NO_LOGGED_ON_USER);
        send(proto::peer::CHANNEL_ID_0, serialize(reply));
        return;
    }

    if (!file_worker_)
    {
        CLOG(ERROR) << "File worker is not available";
        return;
    }

    file_worker_->query(this, clientId(), buffer, [this](QByteArray reply)
    {
        if (!reply.isEmpty())
            send(proto::peer::CHANNEL_ID_0, reply);
    });
}
