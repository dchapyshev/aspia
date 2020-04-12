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

#include "router/session.h"

namespace router {

class SessionPeer : public Session
{
public:
    SessionPeer(std::unique_ptr<net::Channel> channel, std::shared_ptr<Database> database);
    ~SessionPeer();

protected:
    // net::Channel::Listener implementation.
    void onMessageReceived(const base::ByteArray& buffer) override;
    void onMessageWritten() override;

private:
    std::shared_ptr<Database> database_;

    DISALLOW_COPY_AND_ASSIGN(SessionPeer);
};

} // namespace router

#endif // ROUTER__SESSION_PEER_H
