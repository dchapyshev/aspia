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

#ifndef HOST__HOST_SERVER_H
#define HOST__HOST_SERVER_H

#include "base/win/session_status.h"
#include "host/dettach_timer.h"
#include "host/host_settings.h"
#include "host/host_ui_server.h"
#include "host/win/host_process.h"
#include "ipc/ipc_channel.h"
#include "net/network_server.h"

class QFileSystemWatcher;

namespace host {

class PowerSaveBlocker;
class SessionProcess;
struct SrpUserList;

class HostServer : public QObject
{
    Q_OBJECT

public:
    HostServer(QObject* parent = nullptr);
    ~HostServer();

    void start();
    void stop();
    void setSessionEvent(base::win::SessionStatus status, base::win::SessionId session_id);
    void stopSession(const std::string& uuid);

protected:
    // QObject implementation.
    void customEvent(QEvent* event) override;

private slots:
    void onNewConnection();
    void onUiProcessEvent(UiServer::EventType event, base::win::SessionId session_id);
    void onSessionFinished();

private:
    void reloadUsers();
    void startServer();
    void sendConnectEvent(const SessionProcess* session_process);

    enum class State { STOPPED, STOPPING, STARTED };

    State state_ = State::STOPPED;

    Settings settings_;
    std::unique_ptr<QFileSystemWatcher> settings_watcher_;

    std::unique_ptr<UiServer> ui_server_;

    // Accepts incoming network connections.
    std::unique_ptr<net::Server> network_server_;

    DettachTimer dettach_timer_;

    // Contains a list of connected sessions.
    std::list<std::unique_ptr<SessionProcess>> sessions_;

    std::unique_ptr<PowerSaveBlocker> power_save_blocker_;

    DISALLOW_COPY_AND_ASSIGN(HostServer);
};

} // namespace host

#endif // HOST__HOST_SERVER_H
