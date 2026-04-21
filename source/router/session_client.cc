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

#include "router/session_client.h"

#include "base/logging.h"
#include "base/serialization.h"
#include "base/version_constants.h"
#include "base/crypto/random.h"
#include "proto/relay_peer.h"
#include "router/service.h"
#include "router/session_host.h"
#include "router/session_legacy_host.h"
#include "router/session_relay.h"

namespace router {

//--------------------------------------------------------------------------------------------------
SessionClient::SessionClient(base::TcpChannel* channel, QObject* parent)
    : Session(channel, parent)
{
    CLOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
SessionClient::~SessionClient()
{
    CLOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void SessionClient::setStunInfo(quint16 port)
{
    stun_port_ = port;
}

//--------------------------------------------------------------------------------------------------
void SessionClient::onSessionMessage(quint8 channel_id, const QByteArray& buffer)
{
    if (channel_id != proto::router::CHANNEL_ID_CLIENT)
        return;

    proto::router::ClientToRouter message;
    if (!base::parse(buffer, &message))
    {
        CLOG(ERROR) << "Could not read message from client";
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
        CLOG(ERROR) << "Unhandled message from client";
    }
}

//--------------------------------------------------------------------------------------------------
void SessionClient::readConnectionRequest(const proto::router::ConnectionRequest& request)
{
    CLOG(INFO) << "New connection request (host_id:" << request.host_id() << ")";

    proto::router::RouterToClient message;
    proto::router::ConnectionOffer* offer = message.mutable_connection_offer();

    Session* session = sessionByHostId(request.host_id());
    if (!session)
    {
        CLOG(ERROR) << "Host with id" << request.host_id() << "NOT found!";
        offer->set_error_code(proto::router::ConnectionOffer::PEER_NOT_FOUND);
        sendMessage(proto::router::CHANNEL_ID_CLIENT, base::serialize(message));
        return;
    }

    CLOG(INFO) << "Host with id" << request.host_id() << "found";

    std::optional<Service::Credentials> credentials = Service::instance()->takeCredentials();
    if (!credentials.has_value())
    {
        CLOG(ERROR) << "Empty key pool";
        offer->set_error_code(proto::router::ConnectionOffer::KEY_POOL_EMPTY);
        sendMessage(proto::router::CHANNEL_ID_CLIENT, base::serialize(message));
        return;
    }

    SessionRelay* relay = static_cast<SessionRelay*>(Service::instance()->session(credentials->session_id));
    if (!relay)
    {
        CLOG(ERROR) << "No relay with session id" << credentials->session_id;
        offer->set_error_code(proto::router::ConnectionOffer::KEY_POOL_EMPTY);
        sendMessage(proto::router::CHANNEL_ID_CLIENT, base::serialize(message));
        return;
    }

    const std::optional<SessionRelay::PeerData>& peer_data = relay->peerData();
    if (!peer_data.has_value())
    {
        CLOG(ERROR) << "No peer data for relay with session id" << credentials->session_id;
        offer->set_error_code(proto::router::ConnectionOffer::KEY_POOL_EMPTY);
        sendMessage(proto::router::CHANNEL_ID_CLIENT, base::serialize(message));
        return;
    }

    offer->set_error_code(proto::router::ConnectionOffer::SUCCESS);

    proto::router::PeerInfo* peer_info = offer->mutable_peer_info();
    peer_info->set_is_legacy(session->version() < base::kVersion_3_0_0);
    peer_info->set_is_address_equals(session->address() == address());

    if (stun_port_)
    {
        // An empty host string means that the client should use the router's address
        // as the server's host address. This is done to allow for future expansion,
        // but is not currently used.
        proto::router::StunServerInfo* stun_info = offer->mutable_stun_info();
        stun_info->set_version(1);
        stun_info->set_host("");
        stun_info->set_port(stun_port_);
    }

    proto::router::RelayCredentials* offer_credentials = offer->mutable_relay();
    offer_credentials->set_host(relay->peerData()->first);
    offer_credentials->set_port(relay->peerData()->second);
    offer_credentials->mutable_key()->Swap(&credentials->key);

    proto::relay::PeerToRelay::Secret secret;
    secret.set_random_data(base::Random::string(16));
    secret.set_client_address(address().toString().toStdString());
    secret.set_client_user_name(userName().toStdString());
    secret.set_host_address(session->address().toString().toStdString());
    secret.set_host_id(request.host_id());

    offer_credentials->set_secret(secret.SerializeAsString());

    CLOG(INFO) << "Sending connection offer to host";
    SessionHost* host_session = dynamic_cast<SessionHost*>(session);
    if (host_session)
    {
        host_session->sendConnectionOffer(*offer);
    }
    else
    {
        SessionLegacyHost* legacy_host_session = static_cast<SessionLegacyHost*>(session);

        proto::router::legacy::ConnectionOffer legacy_offer;
        legacy_offer.set_peer_role(proto::router::legacy::ConnectionOffer::HOST);
        legacy_offer.set_error_code(proto::router::legacy::ConnectionOffer::SUCCESS);
        legacy_offer.mutable_relay()->CopyFrom(offer->relay());
        legacy_offer.mutable_host_data()->set_host_id(request.host_id());

        legacy_host_session->sendConnectionOffer(legacy_offer);
    }

    CLOG(INFO) << "Sending connection offer to client";
    sendMessage(proto::router::CHANNEL_ID_CLIENT, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void SessionClient::readCheckHostStatus(const proto::router::CheckHostStatus& check_host_status)
{
    proto::router::RouterToClient message;
    proto::router::HostStatus* host_status = message.mutable_host_status();

    if (sessionByHostId(check_host_status.host_id()))
        host_status->set_status(proto::router::HostStatus::STATUS_ONLINE);
    else
        host_status->set_status(proto::router::HostStatus::STATUS_OFFLINE);

    CLOG(INFO) << "Sending host status for host ID" << check_host_status.host_id()
               << ":" << host_status->status();
    sendMessage(proto::router::CHANNEL_ID_CLIENT, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
Session* SessionClient::sessionByHostId(base::HostId host_id)
{
    QList<Session*> session_list = Service::instance()->sessions();

    for (const auto& session : std::as_const(session_list))
    {
        if (session->sessionType() == proto::router::SESSION_TYPE_HOST)
        {
            SessionHost* host_session = dynamic_cast<SessionHost*>(session);
            if (host_session && host_session->hostId() == host_id)
                return host_session;

            SessionLegacyHost* legacy_host_session = dynamic_cast<SessionLegacyHost*>(session);
            if (legacy_host_session && legacy_host_session->hasHostId(host_id))
                return legacy_host_session;
        }
    }

    return nullptr;
}

} // namespace router
