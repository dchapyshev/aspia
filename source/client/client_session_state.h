//
// Aspia Project
// Copyright (C) 2016-2024 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT_CLIENT_SESSION_STATE_H
#define CLIENT_CLIENT_SESSION_STATE_H

#include "base/version.h"
#include "client/client_config.h"

#include <mutex>

namespace client {

class SessionState
{
public:
    explicit SessionState(const Config& config);
    ~SessionState();

    const Config& config() const;
    const std::optional<RouterConfig>& router() const;

    bool isConnectionByHostId() const;

    proto::SessionType sessionType() const;
    const std::u16string& computerName() const;
    const std::u16string& displayName() const;
    const std::u16string& hostAddress() const;
    uint16_t hostPort() const;
    const std::u16string& hostUserName() const;
    const std::u16string& hostPassword() const;

    void setRouterVersion(const base::Version& router_version);
    base::Version routerVersion() const;

    void setHostVersion(const base::Version& host_version);
    base::Version hostVersion() const;

    void setAutoReconnect(bool enable);
    bool isAutoReconnect() const;

    void setReconnecting(bool enable);
    bool isReconnecting() const;

private:
    const Config config_;

    mutable std::mutex lock_;
    base::Version router_version_;
    base::Version host_version_;
    bool auto_reconnect_ = false;
    bool reconnecting_ = false;
};

} // namespace client

#endif // CLIENT_SHARED_SESSION_STATE_H
