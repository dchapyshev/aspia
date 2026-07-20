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

#ifndef HOST_TERMINAL_CLIENT_H
#define HOST_TERMINAL_CLIENT_H

#include <QList>

#include "base/scoped_qpointer.h"
#include "host/client.h"

#include "proto/terminal.h"

class IpcChannel;
class IpcServer;
class Location;

// Host side of a Terminal session. Waits for the operating system credentials supplied by the client,
// validates them, launches the terminal agent process under that account and then bridges the client
// channel to the agent over IPC.
class TerminalClient final : public Client
{
    Q_OBJECT

public:
    TerminalClient(TcpChannel* tcp_channel, QObject* parent = nullptr);
    ~TerminalClient() final;

private slots:
    void onIpcNewConnection();
    void onIpcErrorOccurred();
    void onIpcMessageReceived(quint32 ipc_channel_id, const QByteArray& buffer, bool reliable);
    void onIpcDisconnected();

protected:
    // Client implementation.
    void onStart() final;
    void onMessage(quint8 channel_id, const QByteArray& buffer) final;
    void onTimer(const TimePoint& now) final;

private:
    bool startIpcServer(const QString& ipc_channel_name, const QString& target_user_sid);
    bool launchAgent(const QString& user_name, const QString& password, const QString& ipc_channel_id);
    void sendResult(proto::terminal::Result::Code code);
    void onError(const Location& location);

    ScopedQPointer<IpcServer> ipc_server_;
    ScopedQPointer<IpcChannel> ipc_channel_;

    TimePoint attach_deadline_;
    bool agent_launched_ = false;
    int auth_failure_count_ = 0;

    // Client messages that arrived after the agent was launched but before its IPC channel connected.
    QList<QByteArray> pending_to_agent_;

    Q_DISABLE_COPY_MOVE(TerminalClient)
};

#endif // HOST_TERMINAL_CLIENT_H
