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

#ifndef BASE_PEER_STUN_SERVER_H
#define BASE_PEER_STUN_SERVER_H

#include "base/macros_magic.h"

#include <array>
#include <cstdint>

#include <asio/ip/udp.hpp>

namespace base {

class StunServer
{
public:
    StunServer();
    ~StunServer();

    void start(uint16_t port);

private:
    void doReceiveRequest();
    bool doSendAddressReply();

    asio::ip::udp::socket udp_socket_;
    std::array<uint8_t, 1024> buffer_;

    DISALLOW_COPY_AND_ASSIGN(StunServer);
};

} // namespace base

#endif // BASE_PEER_STUN_SERVER_H
