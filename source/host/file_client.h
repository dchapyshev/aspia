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

#include "base/scoped_qpointer.h"
#include "base/session_id.h"
#include "host/client.h"

class IpcChannel;
class IpcServer;
class Location;

class FileClient final : public Client
{
    Q_OBJECT

public:
    FileClient(TcpChannel* tcp_channel, SessionId session_id, QObject* parent = nullptr);
    ~FileClient() final;

private slots:
    void onIpcNewConnection();
    void onIpcErrorOccurred();

    void onIpcMessageReceived(quint32 ipc_channel_id, const QByteArray& buffer, bool reliable);
    void onIpcDisconnected();

protected:
    void onStart() final;
    void onMessage(quint8 channel_id, const QByteArray& buffer) final;
    void onTimer(const TimePoint& now) final;

private:
    bool startIpcServer(const QString& ipc_channel_name, const QString& target_user_sid);
    void onStarted(const Location& location, bool has_user);
    void onError(const Location& location);

    ScopedQPointer<IpcServer> ipc_server_;
    ScopedQPointer<IpcChannel> ipc_channel_;

    const SessionId session_id_;
    TimePoint attach_deadline_;
    bool has_logged_on_user_ = false;

    Q_DISABLE_COPY_MOVE(FileClient)
};

#endif // HOST_DESKTOP_MANAGER_H
