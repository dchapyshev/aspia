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

#include "host/workers/portable_file_worker.h"

#include "base/logging.h"
#include "common/file_request_handler.h"

//--------------------------------------------------------------------------------------------------
PortableFileWorker::PortableFileWorker()
    : Worker(Thread::AsioDispatcher)
{
    LOG(TRACE) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
PortableFileWorker::~PortableFileWorker()
{
    LOG(TRACE) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void PortableFileWorker::query(QObject* context, quint32 client_id, const QByteArray& buffer,
                               std::function<void(QByteArray)> reply)
{
    Worker::request(context, [this, client_id, buffer]()
    {
        if (!request_.parse<proto::file_transfer::Request>(buffer))
        {
            LOG(ERROR) << "Unable to parse file transfer request";
            return QByteArray();
        }

        std::unique_ptr<FileRequestHandler>& handler = handlers_[client_id];
        if (!handler)
            handler = std::make_unique<FileRequestHandler>();

        handler->doRequest(request_.message<proto::file_transfer::Request>(),
                           &reply_.newMessage<proto::file_transfer::Reply>());
        return reply_.serialize<proto::file_transfer::Reply>();
    },
    std::move(reply));
}

//--------------------------------------------------------------------------------------------------
void PortableFileWorker::removeClient(quint32 client_id)
{
    post([this, client_id]()
    {
        handlers_.erase(client_id);
    });
}

//--------------------------------------------------------------------------------------------------
void PortableFileWorker::onStart()
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void PortableFileWorker::onStop()
{
    handlers_.clear();
}
