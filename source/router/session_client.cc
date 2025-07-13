//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "base/serialization.h"
#include "base/crypto/random.h"
#include "proto/relay_peer.h"
#include "router/server.h"
#include "router/session_host.h"
#include "router/session_relay.h"

namespace router {

//--------------------------------------------------------------------------------------------------
SessionClient::SessionClient(base::TcpChannel* channel, QObject* parent)
    : Session(channel, parent)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
SessionClient::~SessionClient()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void SessionClient::onSessionReady()
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void SessionClient::onSessionMessageReceived(quint8 /* channel_id */, const QByteArray& buffer)
{
    proto::router::PeerToRouter message;
    if (!base::parse(buffer, &message))
    {
        LOG(ERROR) << "Could not read message from client";
        return;
    }

    if (message.has_connection_request())
    {
        readConnectionRequest(message.connection_request());
    }
    else if (message.has_check_host_status())
    {
        readCheckHostStatus(message.check_host_status());
    }
    else
    {
        LOG(ERROR) << "Unhandled message from client";
    }
}

//--------------------------------------------------------------------------------------------------
void SessionClient::onSessionMessageWritten(quint8 /* channel_id */, size_t /* pending */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void SessionClient::readConnectionRequest(const proto::router::ConnectionRequest& request)
{
    LOG(INFO) << "New connection request (host_id:" << request.host_id() << ")";

    proto::router::RouterToPeer message;
    proto::router::ConnectionOffer* offer = message.mutable_connection_offer();

    SessionHost* host = sessionByHostId(request.host_id());
    if (!host)
    {
        LOG(ERROR) << "Host with id" << request.host_id() << "NOT found!";
        offer->set_error_code(proto::router::ConnectionOffer::PEER_NOT_FOUND);
    }
    else
    {
        LOG(INFO) << "Host with id" << request.host_id() << "found";

        std::optional<KeyPool::Credentials> credentials = relayKeyPool().takeCredentials();
        if (!credentials.has_value())
        {
            LOG(ERROR) << "Empty key pool";
            offer->set_error_code(proto::router::ConnectionOffer::KEY_POOL_EMPTY);
        }
        else
        {
            SessionRelay* relay = static_cast<SessionRelay*>(sessionById(credentials->session_id));
            if (!relay)
            {
                LOG(ERROR) << "No relay with session id" << credentials->session_id;
                offer->set_error_code(proto::router::ConnectionOffer::KEY_POOL_EMPTY);
            }
            else
            {
                const std::optional<SessionRelay::PeerData>& peer_data = relay->peerData();
                if (!peer_data.has_value())
                {
                    LOG(ERROR) << "No peer data for relay with session id"
                               << credentials->session_id;
                    offer->set_error_code(proto::router::ConnectionOffer::KEY_POOL_EMPTY);
                }
                else
                {
                    offer->set_error_code(proto::router::ConnectionOffer::SUCCESS);

                    proto::router::HostOfferData* offer_data = offer->mutable_host_data();
                    offer_data->set_host_id(request.host_id());

                    proto::router::RelayCredentials* offer_credentials = offer->mutable_relay();

                    offer_credentials->set_host(relay->peerData()->first);
                    offer_credentials->set_port(relay->peerData()->second);
                    offer_credentials->mutable_key()->Swap(&credentials->key);

                    proto::relay::PeerToRelay::Secret secret;
                    secret.set_random_data(base::Random::string(16));
                    secret.set_client_address(address().toStdString());
                    secret.set_client_user_name(userName().toStdString());
                    secret.set_host_address(host->address().toStdString());
                    secret.set_host_id(request.host_id());

                    offer_credentials->set_secret(secret.SerializeAsString());

                    LOG(INFO) << "Sending connection offer to host";
                    offer->set_peer_role(proto::router::ConnectionOffer::HOST);
                    host->sendConnectionOffer(*offer);
                }
            }
        }
    }

    LOG(INFO) << "Sending connection offer to client";
    offer->clear_host_data(); // Host data is only needed by the host.
    offer->set_peer_role(proto::router::ConnectionOffer::CLIENT);
    sendMessage(base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void SessionClient::readCheckHostStatus(const proto::router::CheckHostStatus& check_host_status)
{
    proto::router::RouterToPeer message;
    proto::router::HostStatus* host_status = message.mutable_host_status();

    if (sessionByHostId(check_host_status.host_id()))
        host_status->set_status(proto::router::HostStatus::STATUS_ONLINE);
    else
        host_status->set_status(proto::router::HostStatus::STATUS_OFFLINE);

    LOG(INFO) << "Sending host status for host ID" << check_host_status.host_id()
              << ":" << host_status->status();
    sendMessage(base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
SessionHost* SessionClient::sessionByHostId(base::HostId host_id)
{
    QList<Session*> session_list = sessions();

    for (const auto& session : std::as_const(session_list))
    {
        if (session->sessionType() == proto::router::SESSION_TYPE_HOST &&
            static_cast<SessionHost*>(session)->hasHostId(host_id))
        {
            return static_cast<SessionHost*>(session);
        }
    }

    return nullptr;
}

} // namespace router
