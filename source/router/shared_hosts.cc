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

#include "router/shared_hosts.h"

//--------------------------------------------------------------------------------------------------
// static
SharedHosts& SharedHosts::instance()
{
    static SharedHosts instance;
    return instance;
}

//--------------------------------------------------------------------------------------------------
bool SharedHosts::contains(HostId host_id) const
{
    std::shared_lock lock(lock_);
    return hosts_.contains(host_id);
}

//--------------------------------------------------------------------------------------------------
std::optional<SharedHosts::Host> SharedHosts::find(HostId host_id) const
{
    std::shared_lock lock(lock_);

    auto it = hosts_.find(host_id);
    if (it == hosts_.end())
        return std::nullopt;

    return it->second;
}

//--------------------------------------------------------------------------------------------------
void SharedHosts::add(HostId host_id, const QVersionNumber& version, const std::string& address)
{
    std::unique_lock lock(lock_);
    hosts_.insert_or_assign(host_id, Host{version, address});
}

//--------------------------------------------------------------------------------------------------
void SharedHosts::remove(HostId host_id)
{
    std::unique_lock lock(lock_);
    hosts_.erase(host_id);
}

//--------------------------------------------------------------------------------------------------
void SharedHosts::clear()
{
    std::unique_lock lock(lock_);
    hosts_.clear();
}
