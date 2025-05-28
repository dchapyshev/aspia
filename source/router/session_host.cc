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

#include "router/session_host.h"

#include "base/logging.h"
#include "base/serialization.h"
#include "base/crypto/generic_hash.h"
#include "base/crypto/random.h"
#include "router/database.h"
#include "router/server.h"

namespace router {

namespace {

const size_t kHostKeySize = 512;

} // namespace

//--------------------------------------------------------------------------------------------------
SessionHost::SessionHost(QObject* parent)
    : Session(proto::ROUTER_SESSION_HOST, parent)
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
SessionHost::~SessionHost()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
bool SessionHost::hasHostId(base::HostId host_id) const
{
    return host_id_list_.contains(host_id);
}

//--------------------------------------------------------------------------------------------------
void SessionHost::sendConnectionOffer(const proto::ConnectionOffer& offer)
{
    proto::RouterToPeer message;
    message.mutable_connection_offer()->CopyFrom(offer);
    sendMessage(proto::ROUTER_CHANNEL_ID_SESSION, message);
}

//--------------------------------------------------------------------------------------------------
void SessionHost::onSessionReady()
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void SessionHost::onSessionMessageReceived(quint8 /* channel_id */, const QByteArray& buffer)
{
    proto::PeerToRouter message;
    if (!base::parse(buffer, &message))
    {
        LOG(LS_ERROR) << "Could not read message from host";
        return;
    }

    if (message.has_host_id_request())
    {
        readHostIdRequest(message.host_id_request());
    }
    else if (message.has_reset_host_id())
    {
        readResetHostId(message.reset_host_id());
    }
    else
    {
        LOG(LS_ERROR) << "Unhandled message from host";
    }
}

//--------------------------------------------------------------------------------------------------
void SessionHost::onSessionMessageWritten(quint8 /* channel_id */, size_t /* pending */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void SessionHost::readHostIdRequest(const proto::HostIdRequest& host_id_request)
{
    std::unique_ptr<Database> database = openDatabase();
    if (!database)
    {
        LOG(LS_ERROR) << "Failed to connect to database";
        return;
    }

    proto::RouterToPeer message;
    proto::HostIdResponse* host_id_response = message.mutable_host_id_response();
    QByteArray key_hash;

    if (host_id_request.type() == proto::HostIdRequest::NEW_ID)
    {
        // Generate new key.
        std::string key = base::Random::string(kHostKeySize);

        // Calculate hash for key.
        key_hash = base::GenericHash::hash(base::GenericHash::Type::BLAKE2b512, key);

        if (!database->addHost(key_hash))
        {
            LOG(LS_ERROR) << "Unable to add host";
            return;
        }

        host_id_response->set_key(std::move(key));
    }
    else if (host_id_request.type() == proto::HostIdRequest::EXISTING_ID)
    {
        // Using existing key.
        key_hash = base::GenericHash::hash(
            base::GenericHash::Type::BLAKE2b512, host_id_request.key());
    }
    else
    {
        LOG(LS_ERROR) << "Unknown request type: " << host_id_request.type();
        return;
    }

    base::HostId host_id = base::kInvalidHostId;

    switch (database->hostId(key_hash, &host_id))
    {
        case Database::ErrorCode::SUCCESS:
        {
            if (host_id != base::kInvalidHostId)
            {
                host_id_response->set_error_code(proto::HostIdResponse::SUCCESS);
                host_id_response->set_host_id(host_id);

                if (!host_id_list_.contains(host_id))
                {
                    host_id_list_.push_back(host_id);
                    removeOtherWithSameId();
                }
            }
            else
            {
                host_id_response->set_error_code(proto::HostIdResponse::UNKNOWN);
                LOG(LS_ERROR) << "Invalid host id";
            }
        }
        break;

        case Database::ErrorCode::NO_HOST_FOUND:
            host_id_response->set_error_code(proto::HostIdResponse::NO_HOST_FOUND);
            break;

        default:
            host_id_response->set_error_code(proto::HostIdResponse::UNKNOWN);
            break;
    }

    sendMessage(proto::ROUTER_CHANNEL_ID_SESSION, message);
}

//--------------------------------------------------------------------------------------------------
void SessionHost::readResetHostId(const proto::ResetHostId& reset_host_id)
{
    base::HostId host_id = reset_host_id.host_id();
    if (host_id == base::kInvalidHostId)
    {
        LOG(LS_ERROR) << "Invalid host ID";
        return;
    }

    if (host_id_list_.empty())
    {
        LOG(LS_ERROR) << "Empty host ID list";
        return;
    }

    for (auto it = host_id_list_.begin(), it_end = host_id_list_.end(); it != it_end; ++it)
    {
        if (*it == host_id)
        {
            LOG(LS_INFO) << "Host ID " << host_id << " remove from list";
            host_id_list_.erase(it);
            return;
        }
    }

    LOG(LS_ERROR) << "Host ID " << host_id << " NOT found in list";
}

//--------------------------------------------------------------------------------------------------
void SessionHost::removeOtherWithSameId()
{
    QList<Session*> session_list = sessions();
    SessionId session_for_stopping = 0;

    for (const auto& other_session : std::as_const(session_list))
    {
        if (other_session->sessionType() != proto::ROUTER_SESSION_HOST || other_session == this)
            continue;

        SessionHost* other_host_session = reinterpret_cast<SessionHost*>(other_session);
        bool is_found = false;

        for (const auto& host_id : std::as_const(host_id_list_))
        {
            if (other_host_session->hasHostId(host_id))
            {
                LOG(LS_INFO) << "Detected previous connection with ID " << host_id;

                is_found = true;
                break;
            }
        }

        if (is_found)
        {
            session_for_stopping = other_session->sessionId();
            break;
        }
    }

    if (session_for_stopping)
        sessionManager()->stopSession(session_for_stopping);
}

} // namespace router
