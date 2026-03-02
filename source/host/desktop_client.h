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

#ifndef HOST_DESKTOP_CLIENT_H
#define HOST_DESKTOP_CLIENT_H

#include <QObject>

#include "base/session_id.h"
#include "base/net/tcp_channel.h"

namespace base {
class IpcChannel;
class IpcServer;
} // namespace base

namespace host {

class DesktopClient final : public QObject
{
    Q_OBJECT

public:
    explicit DesktopClient(base::TcpChannel* tcp_channel, QObject* parent = nullptr);
    ~DesktopClient() final;

    quint32 clientId() const;
    proto::peer::SessionType sessionType() const;
    QString displayName() const;
    QString computerName() const;
    QString userName() const;
    bool isAttached() const;

    QString attach();
    void dettach();

public slots:
    void start();

signals:
    void sig_started(quint32 client_id);
    void sig_finished(quint32 client_id);
    void sig_switchSession(base::SessionId session_id);
    void sig_recordingChanged(const QString& computer, const QString& user, bool started);

private slots:
    void onIpcNewConnection();
    void onIpcErrorOccurred();

    void onIpcMessageReceived(quint32 channel_id, const QByteArray& buffer);
    void onIpcDisconnected();

    void onTcpErrorOccurred(base::TcpChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 tcp_channel_id, const QByteArray& buffer);

private:
    void sendIpcServiceMessage(const QByteArray& buffer);
    void sendSessionList();

    base::IpcServer* ipc_server_ = nullptr;
    base::IpcChannel* ipc_channel_ = nullptr;
    base::TcpChannel* tcp_channel_ = nullptr;
    QTimer* attach_timer_ = nullptr;

    Q_DISABLE_COPY_MOVE(DesktopClient)
};

} // namespace host

#endif // HOST_DESKTOP_CLIENT_H
