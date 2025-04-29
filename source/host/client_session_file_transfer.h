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

#ifndef HOST_CLIENT_SESSION_FILE_TRANSFER_H
#define HOST_CLIENT_SESSION_FILE_TRANSFER_H

#include "base/macros_magic.h"
#include "base/location.h"
#include "base/ipc/ipc_channel.h"
#include "base/ipc/ipc_server.h"
#include "host/client_session.h"

#include <vector>

#include <QPointer>
#include <QTimer>

namespace host {

class ClientSessionFileTransfer final : public ClientSession
{
    Q_OBJECT

public:
    ClientSessionFileTransfer(std::unique_ptr<base::TcpChannel> channel, QObject* parent);
    ~ClientSessionFileTransfer() final;

protected:
    // ClientSession implementation.
    void onStarted() final;
    void onReceived(uint8_t channel_id, const QByteArray& buffer) final;
    void onWritten(uint8_t channel_id, size_t pending) final;

private slots:
    void onIpcDisconnected();
    void onIpcMessageReceived(const QByteArray& buffer);
    void onIpcNewConnection();
    void onIpcErrorOccurred();

private:
    void onError(const base::Location& location);

    QPointer<QTimer> attach_timer_;
    std::unique_ptr<base::IpcServer> ipc_server_;
    std::unique_ptr<base::IpcChannel> ipc_channel_;
    std::vector<QByteArray> pending_messages_;
    bool has_logged_on_user_ = false;

    DISALLOW_COPY_AND_ASSIGN(ClientSessionFileTransfer);
};

} // namespace host

#endif // HOST_CLIENT_SESSION_FILE_TRANSFER_H
