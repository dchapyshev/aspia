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

#include "client/client_session_state.h"

#include "base/peer/host_id.h"

namespace client {

//--------------------------------------------------------------------------------------------------
SessionState::SessionState(const Config& config)
    : config_(config)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
SessionState::~SessionState() = default;

//--------------------------------------------------------------------------------------------------
const Config& SessionState::config() const
{
    return config_;
}

//--------------------------------------------------------------------------------------------------
const std::optional<RouterConfig>& SessionState::router() const
{
    return config_.router_config;
}

//--------------------------------------------------------------------------------------------------
bool SessionState::isConnectionByHostId() const
{
    return base::isHostId(config_.address_or_id);
}

//--------------------------------------------------------------------------------------------------
proto::peer::SessionType SessionState::sessionType() const
{
    return config_.session_type;
}

//--------------------------------------------------------------------------------------------------
const QString& SessionState::computerName() const
{
    return config_.computer_name;
}

//--------------------------------------------------------------------------------------------------
const QString& SessionState::displayName() const
{
    return config_.display_name;
}

//--------------------------------------------------------------------------------------------------
const QString& SessionState::hostAddress() const
{
    return config_.address_or_id;
}

//--------------------------------------------------------------------------------------------------
quint16 SessionState::hostPort() const
{
    return config_.port;
}

//--------------------------------------------------------------------------------------------------
const QString& SessionState::hostUserName() const
{
    return config_.username;
}

//--------------------------------------------------------------------------------------------------
const QString& SessionState::hostPassword() const
{
    return config_.password;
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
