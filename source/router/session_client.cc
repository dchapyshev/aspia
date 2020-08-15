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

#include "router/session_client.h"

#include "base/logging.h"

namespace router {

SessionClient::SessionClient(std::unique_ptr<base::NetworkChannel> channel,
                             std::shared_ptr<DatabaseFactory> database_factory,
                             std::shared_ptr<ServerProxy> server_proxy)
    : Session(proto::ROUTER_SESSION_CLIENT, std::move(channel), std::move(database_factory)),
      server_proxy_(std::move(server_proxy))
{
    DCHECK(server_proxy_);
}

SessionClient::~SessionClient() = default;

void SessionClient::onSessionReady()
{
    // Nothing
}

void SessionClient::onMessageReceived(const base::ByteArray& buffer)
{
    proto::ClientToRouter message;
    if (!base::parse(buffer, &message))
    {
        LOG(LS_ERROR) << "Could not read message from client";
        return;
    }

    if (message.has_connection_request())
    {
        LOG(LS_INFO) << "CONNECTION REQUEST: " << message.connection_request().host_id();
    }
    else
    {
        LOG(LS_WARNING) << "Unhandled message from client";
    }
}

void SessionClient::onMessageWritten(size_t /* pending */)
{
    // Nothing
}

} // namespace router
