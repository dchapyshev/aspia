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

#include "client/session_state.h"

#include "base/net/address.h"
#include "base/peer/host_id.h"
#include "build/build_config.h"

namespace client {

//--------------------------------------------------------------------------------------------------
SessionState::SessionState(const ComputerConfig& computer,
                           proto::peer::SessionType session_type,
                           const QString& display_name)
    : computer_(computer),
      session_type_(session_type),
      display_name_(display_name)
{
    if (base::isHostId(computer_.address))
    {
        host_address_ = computer_.address;
        host_port_ = 0;
    }
    else
    {
        base::Address address = base::Address::fromString(computer_.address, DEFAULT_HOST_TCP_PORT);
        host_address_ = address.host();
        host_port_ = address.port();
    }
}

//--------------------------------------------------------------------------------------------------
bool SessionState::isConnectionByHostId() const
{
    return base::isHostId(computer_.address);
}

//--------------------------------------------------------------------------------------------------
base::HostId SessionState::hostId() const
{
    if (!base::isHostId(computer_.address))
        return base::kInvalidHostId;

    return base::stringToHostId(computer_.address);
}

//--------------------------------------------------------------------------------------------------
void SessionState::setHostUserName(const QString& username)
{
    computer_.username = username;
}

//--------------------------------------------------------------------------------------------------
void SessionState::setHostPassword(const QString& password)
{
    computer_.password = password;
}

//--------------------------------------------------------------------------------------------------
void SessionState::setRouterVersion(const QVersionNumber& router_version)
{
    std::scoped_lock lock(lock_);
    router_version_ = router_version;
}

//--------------------------------------------------------------------------------------------------
QVersionNumber SessionState::routerVersion() const
{
    std::scoped_lock lock(lock_);
    return router_version_;
}

//--------------------------------------------------------------------------------------------------
void SessionState::setHostVersion(const QVersionNumber& host_version)
{
    std::scoped_lock lock(lock_);
    host_version_ = host_version;
}

//--------------------------------------------------------------------------------------------------
QVersionNumber SessionState::hostVersion() const
{
    std::scoped_lock lock(lock_);
    return host_version_;
}

//--------------------------------------------------------------------------------------------------
void SessionState::setAutoReconnect(bool enable)
{
    std::scoped_lock lock(lock_);
    auto_reconnect_ = enable;
}

//--------------------------------------------------------------------------------------------------
bool SessionState::isAutoReconnect() const
{
    std::scoped_lock lock(lock_);
    return auto_reconnect_;
}

//--------------------------------------------------------------------------------------------------
void SessionState::setReconnecting(bool enable)
{
    std::scoped_lock lock(lock_);
    reconnecting_ = enable;
}

//--------------------------------------------------------------------------------------------------
bool SessionState::isReconnecting() const
{
    std::scoped_lock lock(lock_);
    return reconnecting_;
}

} // namespace client
