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

#ifndef ROUTER__SESSION_PEER_H
#define ROUTER__SESSION_PEER_H

#include "proto/router.pb.h"
#include "router/session.h"
#include "peer/peer_id.h"

namespace router {

class SessionPeer : public Session
{
public:
    SessionPeer(proto::RouterSession session_type,
                std::unique_ptr<base::NetworkChannel> channel,
                std::shared_ptr<Database> database);
    ~SessionPeer();

protected:
    // net::Channel::Listener implementation.
    void onMessageReceived(const base::ByteArray& buffer) override;
    void onMessageWritten(size_t pending) override;

private:
    const proto::RouterSession session_type_;
    peer::PeerId peer_id_ = peer::kInvalidPeerId;
    std::shared_ptr<Database> database_;

    DISALLOW_COPY_AND_ASSIGN(SessionPeer);
};

} // namespace router

#endif // ROUTER__SESSION_PEER_H
