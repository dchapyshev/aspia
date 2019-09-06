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

#include "base/macros_magic.h"
#include "client/client.h"
#include "client/file_request_consumer.h"

namespace common {
class FileWorker;
} // namespace common

namespace client {

class FileRequestProducerProxy;

class ClientFileTransfer
    : public Client,
      public FileRequestConsumer
{
public:
    explicit ClientFileTransfer(const Config& config);
    ~ClientFileTransfer();

    // FileRequestConsumer implementation.
    void localRequest(const proto::FileRequest& request) override;
    void remoteRequest(const proto::FileRequest& request) override;

protected:
    // Client implementation.
    void onMessageReceived(const base::ByteArray& buffer) override;

private:
    void onSessionError(const QString& message);

    std::unique_ptr<common::FileWorker> local_worker_;
    std::unique_ptr<FileRequestProducerProxy> request_producer_;

    DISALLOW_COPY_AND_ASSIGN(ClientFileTransfer);
};

} // namespace client

#endif // CLIENT__CLIENT_FILE_TRANSFER_H
