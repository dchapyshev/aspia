//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ROUTER__SESSION_HOST_H
#define ROUTER__SESSION_HOST_H

#include "base/peer/host_id.h"
#include "proto/router_host.pb.h"
#include "router/session.h"

namespace router {

class ServerProxy;

class SessionHost : public Session
{
public:
    SessionHost(std::unique_ptr<base::NetworkChannel> channel,
                std::shared_ptr<DatabaseFactory> database_factory,
                std::shared_ptr<ServerProxy> server_proxy);
    ~SessionHost();

    base::HostId hostId() const { return host_id_; }

protected:
    // Session implementation.
    void onSessionReady() override;

    // base::NetworkChannel::Listener implementation.
    void onMessageReceived(const base::ByteArray& buffer) override;
    void onMessageWritten(size_t pending) override;

private:
    void readHostIdRequest(const proto::HostIdRequest& host_id_request);

    std::shared_ptr<ServerProxy> server_proxy_;
    base::HostId host_id_ = base::kInvalidHostId;

    DISALLOW_COPY_AND_ASSIGN(SessionHost);
};

} // namespace router

#endif // ROUTER__SESSION_HOST_H
