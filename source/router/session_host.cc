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

#include "router/session_host.h"

#include "base/logging.h"
#include "base/serialization.h"
#include "base/crypto/generic_hash.h"
#include "base/crypto/random.h"
#include "router/database.h"
#include "router/service.h"

namespace router {

namespace {

const size_t kHostKeySize = 512;

} // namespace

//--------------------------------------------------------------------------------------------------
SessionHost::SessionHost(base::TcpChannel* channel, QObject* parent)
    : Session(channel, parent)
{
    CLOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
SessionHost::~SessionHost()
{
    CLOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void SessionHost::sendConnectionOffer(const proto::router::ConnectionOffer& offer)
{
    proto::router::RouterToHost message;
    message.mutable_connection_offer()->CopyFrom(offer);
    sendMessage(0, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void SessionHost::sendHostCommand(const proto::router::HostCommand& command)
{
    proto::router::RouterToHost message;
    message.mutable_host_command()->CopyFrom(command);
    sendMessage(0, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void SessionHost::onSessionMessage(quint8 channel_id, const QByteArray& buffer)
{
    proto::router::HostToRouter message;
    if (!base::parse(buffer, &message))
    {
        CLOG(ERROR) << "Could not read message from host";
        return;
    }

    if (message.has_host_id_request())
    {
        readHostIdRequest(message.host_id_request());
    }
    else
    {
        CLOG(ERROR) << "Unhandled message from host";
    }
}

//--------------------------------------------------------------------------------------------------
void SessionHost::readHostIdRequest(const proto::router::HostIdRequest& host_id_request)
{
    Database database = Database::open();
    if (!database.isValid())
    {
        CLOG(ERROR) << "Failed to connect to database";
        return;
    }

    proto::router::RouterToHost message;
    proto::router::HostIdResponse* host_id_response = message.mutable_host_id_response();
    QByteArray key_hash;

    if (host_id_request.type() == proto::router::HostIdRequest::NEW_ID)
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
    else if (host_id_request.type() == proto::router::HostIdRequest::EXISTING_ID)
    {
        // Using existing key.
        key_hash = base::GenericHash::hash(base::GenericHash::Type::BLAKE2b512, host_id_request.key());
    }
    else
    {
        CLOG(ERROR) << "Unknown request type:" << host_id_request.type();
        return;
    }

    switch (database.hostId(key_hash, &host_id_))
    {
        case Database::ErrorCode::SUCCESS:
        {
            if (host_id_ != base::kInvalidHostId)
            {
                host_id_response->set_error_code("ok");
                host_id_response->set_host_id(host_id_);
                emit sig_hostIdAssigned(host_id_);
            }
            else
            {
                host_id_response->set_error_code("unknown");
                CLOG(ERROR) << "Invalid host id";
            }
        }
        break;

        case Database::ErrorCode::NO_HOST_FOUND:
            host_id_response->set_error_code("no_host_found");
            break;

        default:
            host_id_response->set_error_code("unknown");
            break;
    }

    sendMessage(0, base::serialize(message));
}

} // namespace router
