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
#include "base/crypto/random.h"
#include "base/strings/unicode.h"
#include "router/server.h"
#include "router/session_host.h"

namespace router {

SessionClient::SessionClient()
    : Session(proto::ROUTER_SESSION_CLIENT)
{
    // Nothing
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
        readConnectionRequest(message.connection_request());
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

void SessionClient::readConnectionRequest(const proto::ConnectionRequest& request)
{
    proto::RouterToClient message;
    proto::ConnectionOffer* offer = message.mutable_connection_offer();

    SessionHost* host = server().hostSessionById(request.host_id());
    if (!host)
    {
        offer->set_error_code(proto::ConnectionOffer::PEER_NOT_FOUND);
    }
    else
    {
        std::optional<SharedKeyPool::Credentials> credentials = relayKeyPool().takeCredentials();
        if (!credentials.has_value())
        {
            offer->set_error_code(proto::ConnectionOffer::KEY_POOL_EMPTY);
        }
        else
        {
            offer->set_error_code(proto::ConnectionOffer::SUCCESS);

            proto::RelayCredentials* offer_credentials = offer->mutable_relay();

            offer_credentials->set_host(base::utf8FromUtf16(credentials->host));
            offer_credentials->set_port(credentials->port);
            offer_credentials->mutable_key()->CopyFrom(credentials->key);
            offer_credentials->set_secret(base::Random::string(64));

            host->sendConnectionOffer(*offer);
        }
    }

    sendMessage(message);
}

} // namespace router
