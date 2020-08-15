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

#ifndef ROUTER__SESSION_RELAY_H
#define ROUTER__SESSION_RELAY_H

#include "proto/router_relay.pb.h"
#include "router/session.h"

namespace router {

class SessionRelay : public Session
{
public:
    SessionRelay(std::unique_ptr<base::NetworkChannel> channel,
                 std::shared_ptr<DatabaseFactory> database_factory);
    ~SessionRelay();

    uint32_t poolSize() const;

protected:
    // Session implementation.
    void onSessionReady() override;

    // net::Channel::Listener implementation.
    void onMessageReceived(const base::ByteArray& buffer) override;
    void onMessageWritten(size_t pending) override;

private:
    void sendKeyPoolRequest(uint32_t pool_size);
    void readKeyPool(const proto::RelayKeyPool& key_pool);

    std::vector<proto::RelayKey> pool_;

    DISALLOW_COPY_AND_ASSIGN(SessionRelay);
};

} // namespace router

#endif // ROUTER__SESSION_RELAY_H
