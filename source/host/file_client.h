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

#ifndef HOST_FILE_CLIENT_H
#define HOST_FILE_CLIENT_H

#include "base/session_id.h"
#include "host/client.h"

class QTimer;

namespace base {
class IpcChannel;
class IpcServer;
class Location;
} // namespace base

namespace host {

class FileClient final : public Client
{
    Q_OBJECT

public:
    FileClient(base::TcpChannel* tcp_channel, base::SessionId session_id, QObject* parent = nullptr);
    ~FileClient() final;

    void start() final;

private slots:
    void onIpcNewConnection();
    void onIpcErrorOccurred();

    void onIpcMessageReceived(quint32 ipc_channel_id, const QByteArray& buffer);
    void onIpcDisconnected();

protected:
    void onMessage(quint8 channel_id, const QByteArray& buffer) final;

private:
    bool startIpcServer(const QString& ipc_channel_name);
    void onStarted(const base::Location& location, bool has_user);
    void onError(const base::Location& location);

    base::IpcServer* ipc_server_ = nullptr;
    base::IpcChannel* ipc_channel_ = nullptr;

    const base::SessionId session_id_;
    QTimer* attach_timer_ = nullptr;
    bool has_logged_on_user_ = false;

    Q_DISABLE_COPY_MOVE(FileClient)
};

} // namespace host

#endif // HOST_DESKTOP_MANAGER_H
