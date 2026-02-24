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

#include <QObject>

#include "base/location.h"
#include "base/session_id.h"
#include "base/ipc/ipc_channel.h"
#include "base/net/tcp_channel.h"

namespace host {

class FileClient final : public QObject
{
    Q_OBJECT

public:
    explicit FileClient(base::TcpChannel* tcp_channel, QObject* parent = nullptr);
    ~FileClient() final;

public slots:
    void start(base::SessionId session_id);

signals:
    void sig_started();
    void sig_finished();

private slots:
    void onIpcNewConnection();
    void onIpcErrorOccurred();

    void onIpcMessageReceived(quint32 ipc_channel_id, const QByteArray& buffer);
    void onIpcDisconnected();

    void onTcpErrorOccurred(base::TcpChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 tcp_channel_id, const QByteArray& buffer);

private:
    bool startIpcServer(const QString& ipc_channel_name);
    void onStarted(const base::Location& location, bool has_user);
    void onError(const base::Location& location);

    base::TcpChannel* tcp_channel_ = nullptr;

    base::IpcServer* ipc_server_ = nullptr;
    base::IpcChannel* ipc_channel_ = nullptr;

    QTimer* attach_timer_ = nullptr;

    bool has_logged_on_user_ = false;

    Q_DISABLE_COPY_MOVE(FileClient)
};

} // namespace host

#endif // HOST_DESKTOP_MANAGER_H
