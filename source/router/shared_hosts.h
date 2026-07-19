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

#ifndef ROUTER_SHARED_HOSTS_H
#define ROUTER_SHARED_HOSTS_H

#include <QVersionNumber>

#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "base/peer/host_id.h"

// Thread-safe registry of the hosts currently served by the host worker. The host worker updates
// it as sessions come and go; any thread reads it through a shared lock.
class SharedHosts final
{
public:
    static SharedHosts& instance();

    struct Host
    {
        QVersionNumber version;
        std::string address;
    };

    bool contains(HostId host_id) const;
    std::optional<Host> find(HostId host_id) const;

    void add(HostId host_id, const QVersionNumber& version, const std::string& address);
    void remove(HostId host_id);
    void clear();

private:
    SharedHosts() = default;
    ~SharedHosts() = default;

    mutable std::shared_mutex lock_;
    std::unordered_map<HostId, Host> hosts_;

    Q_DISABLE_COPY_MOVE(SharedHosts)
};

#endif // ROUTER_SHARED_HOSTS_H
