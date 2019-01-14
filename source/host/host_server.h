//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_HOST__HOST_SERVER_H
#define ASPIA_HOST__HOST_SERVER_H

#include "host/win/host_process.h"
#include "ipc/ipc_channel.h"
#include "network/network_server.h"

namespace aspia {

class Host;
struct SrpUserList;

class HostServer : public QObject
{
    Q_OBJECT

public:
    HostServer(QObject* parent = nullptr);
    ~HostServer();

    bool start();
    void stop();
    void setSessionChanged(uint32_t event, uint32_t session_id);

signals:
    void sessionChanged(uint32_t event, uint32_t session_id);

private slots:
    void onNewConnection();
    void onHostFinished(Host* host);
    void onIpcServerStarted(const QString& channel_id);
    void onIpcNewConnection(ipc::Channel* channel);
    void onIpcMessageReceived(const QByteArray& buffer);
    void onNotifierProcessError(HostProcess::ErrorCode error_code);
    void restartNotifier();

private:
    enum class NotifierState { STOPPED, STARTING, STARTED };

    void startNotifier();
    void stopNotifier();
    void sessionToNotifier(const Host& host);
    void sessionCloseToNotifier(const Host& host);

    // Accepts incoming network connections.
    QPointer<NetworkServer> network_server_;

    // Contains the status of the notifier process.
    NotifierState notifier_state_ = NotifierState::STOPPED;

    // Starts and monitors the status of the notifier process.
    QPointer<HostProcess> notifier_process_;

    bool has_user_session_ = true;

    // The channel is used to communicate with the notifier process.
    QPointer<ipc::Channel> ipc_channel_;

    // Contains a list of connected sessions.
    QList<QPointer<Host>> session_list_;

    DISALLOW_COPY_AND_ASSIGN(HostServer);
};

} // namespace aspia

#endif // ASPIA_HOST__HOST_SERVER_H
