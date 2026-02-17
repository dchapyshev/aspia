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

#ifndef BASE_PEER_STUN_PEER_H
#define BASE_PEER_STUN_PEER_H

#include "base/location.h"
#include "base/macros_magic.h"
#include "base/waitable_timer.h"
#include "proto/stun_peer.pb.h"

#include <cstdint>
#include <memory>
#include <string>

#include <asio/ip/udp.hpp>

namespace base {

class StunPeer
{
public:
    StunPeer();
    ~StunPeer();

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onStunExternalEndpoint(
            asio::ip::udp::socket socket, const asio::ip::udp::endpoint& endpoint) = 0;
        virtual void onStunErrorOccurred() = 0;
    };

    void start(std::u16string_view stun_host, uint16_t stun_port, Delegate* delegate);

private:
    void doStart();
    void doStop();
    void doReceiveExternalAddress();
    void onErrorOccurred(const Location& location, const std::error_code& error_code);

    Delegate* delegate_ = nullptr;
    base::WaitableTimer timer_;
    int number_of_attempts_ = 0;

    std::string stun_host_;
    std::string stun_port_;

    asio::ip::udp::resolver udp_resolver_;
    asio::ip::udp::socket udp_socket_;
    std::array<uint8_t, 1024> buffer_;

    DISALLOW_COPY_AND_ASSIGN(StunPeer);
};

} // namespace base

#endif // BASE_PEER_STUN_PEER_H
