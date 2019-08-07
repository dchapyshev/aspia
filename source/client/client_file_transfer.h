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

#ifndef CLIENT__CLIENT_FILE_TRANSFER_H
#define CLIENT__CLIENT_FILE_TRANSFER_H

#include "client/client.h"
#include "client/connect_data.h"
#include "common/file_request.h"
#include "proto/file_transfer.pb.h"

#include <QQueue>

namespace common {
class FileWorker;
} // namespace common

namespace client {

Q_DECLARE_METATYPE(proto::file_transfer::Request);
Q_DECLARE_METATYPE(proto::file_transfer::Reply);

class ClientFileTransfer : public Client
{
    Q_OBJECT

public:
    ClientFileTransfer(const ConnectData& connect_data, QObject* parent);
    ~ClientFileTransfer();

    common::FileWorker* localWorker();

public slots:
    void remoteRequest(common::FileRequest* request);

protected:
    // net::Listener implementation.
    void onNetworkMessage(const QByteArray& buffer) override;

private:
    void onSessionError(const QString& message);

    QScopedPointer<common::FileWorker> worker_;
    QThread* worker_thread_;

    QQueue<QPointer<common::FileRequest>> requests_;

    DISALLOW_COPY_AND_ASSIGN(ClientFileTransfer);
};

} // namespace client

#endif // CLIENT__CLIENT_FILE_TRANSFER_H
