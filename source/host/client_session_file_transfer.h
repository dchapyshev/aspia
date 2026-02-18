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

#ifndef HOST_CLIENT_SESSION_FILE_TRANSFER_H
#define HOST_CLIENT_SESSION_FILE_TRANSFER_H

#include <QList>
#include <QTimer>

#include "base/location.h"
#include "base/ipc/ipc_channel.h"
#include "base/ipc/ipc_server.h"
#include "host/client_session.h"

namespace host {

class ClientConnectionFileTransfer final : public ClientConnection
{
    Q_OBJECT

public:
    ClientConnectionFileTransfer(base::TcpChannel* channel, QObject* parent);
    ~ClientConnectionFileTransfer() final;

protected:
    // ClientConnection implementation.
    void onStarted() final;
    void onReceived(const QByteArray& buffer) final;

private slots:
    void onIpcDisconnected();
    void onIpcMessageReceived(const QByteArray& buffer);
    void onIpcNewConnection();
    void onIpcErrorOccurred();

private:
    void onError(const base::Location& location);

    QTimer* attach_timer_ = nullptr;
    base::IpcServer* ipc_server_ = nullptr;
    base::IpcChannel* ipc_channel_ = nullptr;
    QList<QByteArray> pending_messages_;
    bool has_logged_on_user_ = false;

    Q_DISABLE_COPY(ClientConnectionFileTransfer)
};

} // namespace host

#endif // HOST_CLIENT_SESSION_FILE_TRANSFER_H
