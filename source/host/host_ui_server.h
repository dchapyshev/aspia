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

#ifndef HOST__HOST_UI_SERVER_H
#define HOST__HOST_UI_SERVER_H

#include <QObject>
#include <QPointer>

#include "base/macros_magic.h"
#include "base/win/session_id.h"
#include "base/win/session_status.h"
#include "proto/notifier.pb.h"

namespace ipc {
class Channel;
class Server;
} // namespace ipc

namespace host {

class UiProcess;

class UiServer : public QObject
{
    Q_OBJECT

public:
    explicit UiServer(QObject* parent = nullptr);
    ~UiServer();

    enum class State { STOPPED, STOPPING, STARTED };
    enum class ProcessEvent { CONNECTED, DISCONNECTED };

    bool start();
    void stop();

    State state() const { return state_; }

    bool hasUiForSession(base::win::SessionId session_id) const;

    void setSessionEvent(base::win::SessionStatus status, base::win::SessionId session_id);
    void setConnectEvent(base::win::SessionId session_id, const proto::notifier::ConnectEvent& event);
    void setDisconnectEvent(base::win::SessionId session_id, const std::string& uuid);

signals:
    void processEvent(ProcessEvent event, base::win::SessionId session_id);
    void killSession(const std::string& uuid);
    void finished();

private slots:
    void onChannelConnected(ipc::Channel* channel);
    void onProcessFinished();

private:
    State state_ = State::STOPPED;

    // IPC server accepts incoming connections from UI processes.
    QPointer<ipc::Server> server_;

    // List of connected UI processes.
    std::list<UiProcess*> process_list_;

    DISALLOW_COPY_AND_ASSIGN(UiServer);
};

} // namespace host

#endif // HOST__HOST_UI_SERVER_H
