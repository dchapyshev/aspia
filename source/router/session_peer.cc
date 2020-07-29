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

#include "router/session_peer.h"

#include "base/logging.h"
#include "base/net/network_channel.h"

namespace router {

SessionPeer::SessionPeer(proto::RouterSession session_type,
                         std::unique_ptr<base::NetworkChannel> channel,
                         std::shared_ptr<Database> database)
    : Session(std::move(channel)),
      session_type_(session_type),
      database_(std::move(database))
{
    // Nothing
}

SessionPeer::~SessionPeer() = default;

void SessionPeer::onMessageReceived(const base::ByteArray& buffer)
{
    proto::PeerToRouter message;

    if (!base::parse(buffer, &message))
    {
        LOG(LS_ERROR) << "Could not read message from peer";
        return;
    }

    if (message.has_peer_id_request())
    {
        LOG(LS_INFO) << "PEER ID REQUEST";
    }
    else if (message.has_connection_request())
    {
        LOG(LS_INFO) << "CONNECTION REQUEST";
    }
    else if (message.has_connection_candidate())
    {
        LOG(LS_INFO) << "CONNECTION CANDIDATE";
    }
    else
    {
        LOG(LS_WARNING) << "Unhandled message from peer";
    }

    // TODO
}

void SessionPeer::onMessageWritten(size_t pending)
{
    // Nothing
}

} // namespace router
