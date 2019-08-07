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

#include "base/win/session_id.h"
#include "base/win/session_status.h"
#include "host/host_settings.h"
#include "net/network_server.h"

class QFileSystemWatcher;

namespace host {

class HostServer
{
public:
    HostServer();
    ~HostServer();

    void start();
    void setSessionEvent(base::win::SessionStatus status, base::win::SessionId session_id);

private:
    std::unique_ptr<QFileSystemWatcher> settings_watcher_;
    Settings settings_;

    // Accepts incoming network connections.
    std::unique_ptr<net::Server> network_server_;

    DISALLOW_COPY_AND_ASSIGN(HostServer);
};

} // namespace host

#endif // HOST__HOST_SERVER_H
