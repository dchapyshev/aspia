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

#include "router/session_legacy_host.h"

#include "base/logging.h"
#include "base/serialization.h"
#include "base/crypto/generic_hash.h"
#include "base/crypto/random.h"
#include "router/database.h"

namespace router {

namespace {

const size_t kHostKeySize = 512;

} // namespace

//--------------------------------------------------------------------------------------------------
SessionLegacyHost::SessionLegacyHost(base::TcpChannel* channel, QObject* parent)
    : Session(channel, parent)
{
    CLOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
SessionLegacyHost::~SessionLegacyHost()
{
    CLOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
bool SessionLegacyHost::hasHostId(base::HostId host_id) const
{
    return host_id_list_.contains(host_id);
}

//--------------------------------------------------------------------------------------------------
void SessionLegacyHost::sendConnectionOffer(const proto::router::legacy::ConnectionOffer& offer)
{
    proto::router::legacy::RouterToPeer message;
    message.mutable_connection_offer()->CopyFrom(offer);
    sendMessage(base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void SessionLegacyHost::onSessionMessage(const QByteArray& buffer)
{
    proto::router::legacy::PeerToRouter message;
    if (!base::parse(buffer, &message))
    {
        CLOG(ERROR) << "Could not read message from host";
        return;
    }

    if (message.has_host_id_request())
        readHostIdRequest(message.host_id_request());
    else if (message.has_reset_host_id())
        readResetHostId(message.reset_host_id());
    else
        CLOG(ERROR) << "Unhandled message from host";
}

//--------------------------------------------------------------------------------------------------
void SessionLegacyHost::readHostIdRequest(const proto::router::legacy::HostIdRequest& host_id_request)
{
    Database database = Database::open();
    if (!database.isValid())
    {
        CLOG(ERROR) << "Failed to connect to database";
        return;
    }

    proto::router::legacy::RouterToPeer message;
    proto::router::legacy::HostIdResponse* host_id_response = message.mutable_host_id_response();
    QByteArray key_hash;

    if (host_id_request.type() == proto::router::legacy::HostIdRequest::NEW_ID)
    {
        // Generate new key.
        std::string key = base::Random::string(kHostKeySize);

        // Calculate hash for key.
        key_hash = base::GenericHash::hash(base::GenericHash::Type::BLAKE2b512, key);

        if (!database.addHost(key_hash))
        {
            CLOG(ERROR) << "Unable to add host";
            return;
        }

        host_id_response->set_key(std::move(key));
    }
    else if (host_id_request.type() == proto::router::legacy::HostIdRequest::EXISTING_ID)
    {
        // Using existing key.
        key_hash = base::GenericHash::hash(base::GenericHash::Type::BLAKE2b512, host_id_request.key());
    }
    else
    {
        CLOG(ERROR) << "Unknown request type:" << host_id_request.type();
        return;
    }

    base::HostId host_id = base::kInvalidHostId;

    switch (database.hostId(key_hash, &host_id))
    {
        case Database::ErrorCode::SUCCESS:
        {
            if (host_id != base::kInvalidHostId)
            {
                host_id_response->set_error_code(proto::router::legacy::HostIdResponse::SUCCESS);
                host_id_response->set_host_id(host_id);

                if (!host_id_list_.contains(host_id))
                {
                    host_id_list_.emplace_back(host_id);
                    emit sig_hostIdAssigned(host_id);
                }
            }
            else
            {
                host_id_response->set_error_code(proto::router::legacy::HostIdResponse::UNKNOWN);
                CLOG(ERROR) << "Invalid host id";
            }
        }
        break;

        case Database::ErrorCode::NO_HOST_FOUND:
            host_id_response->set_error_code(proto::router::legacy::HostIdResponse::NO_HOST_FOUND);
            break;

        default:
            host_id_response->set_error_code(proto::router::legacy::HostIdResponse::UNKNOWN);
            break;
    }

    sendMessage(base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void SessionLegacyHost::readResetHostId(const proto::router::legacy::ResetHostId& reset_host_id)
{
    base::HostId host_id = reset_host_id.host_id();
    if (host_id == base::kInvalidHostId)
    {
        CLOG(ERROR) << "Invalid host ID";
        return;
    }

    if (host_id_list_.isEmpty())
    {
        CLOG(ERROR) << "Empty host ID list";
        return;
    }

    for (auto it = host_id_list_.begin(), it_end = host_id_list_.end(); it != it_end; ++it)
    {
        if (*it == host_id)
        {
            CLOG(INFO) << "Host ID" << host_id << "remove from list";
            host_id_list_.erase(it);
            return;
        }
    }

    CLOG(ERROR) << "Host ID" << host_id << "is NOT found in list";
}

} // namespace router
