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

#ifndef HOST_WORKERS_PORTABLE_FILE_WORKER_H
#define HOST_WORKERS_PORTABLE_FILE_WORKER_H

#include <functional>
#include <memory>
#include <unordered_map>

#include "base/serialization.h"
#include "base/threading/worker.h"
#include "proto/file_transfer.h"

class FileRequestHandler;

// File operations of the portable host running with the rights of the user.
class PortableFileWorker final : public Worker
{
    Q_OBJECT

public:
    PortableFileWorker();
    ~PortableFileWorker() final;

    // Processes the serialized request in |buffer| of the client |client_id| in the worker thread and
    // delivers the serialized reply (empty when the request is malformed) to |reply| in the calling
    // worker's thread. The reply is dropped if |context| is destroyed before it is ready. May be
    // called from any worker thread.
    void query(QObject* context, quint32 client_id, const QByteArray& buffer,
               std::function<void(QByteArray)> reply);

    // Drops the state of the client |client_id| (a transfer in progress). May be called from any
    // thread.
    void removeClient(quint32 client_id);

protected:
    // Worker implementation.
    void onStart() final;
    void onStop() final;

private:
    std::unordered_map<quint32, std::unique_ptr<FileRequestHandler>> handlers_;

    Parser<proto::file_transfer::Request> request_;
    Serializer<proto::file_transfer::Reply> reply_;

    Q_DISABLE_COPY_MOVE(PortableFileWorker)
};

#endif // HOST_WORKERS_PORTABLE_FILE_WORKER_H
