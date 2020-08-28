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

#ifndef BASE__PEER__RELAY_PEER_H
#define BASE__PEER__RELAY_PEER_H

#include "base/macros_magic.h"
#include "base/memory/byte_array.h"
#include "proto/router_common.pb.h"

#include <asio/ip/tcp.hpp>

namespace base {

class NetworkChannel;
class Location;

class RelayPeer
{
public:
    RelayPeer();
    ~RelayPeer();

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onRelayConnectionReady(std::unique_ptr<NetworkChannel> channel) = 0;
        virtual void onRelayConnectionError() = 0;
    };

    void start(const proto::RelayCredentials& credentials, Delegate* delegate);
    bool isFinished() const { return is_finished_; }

private:
    void onConnected();
    void onErrorOccurred(const Location& location, const std::error_code& error_code);

    static ByteArray authenticationMessage(const proto::RelayKey& key, const std::string& secret);

    Delegate* delegate_ = nullptr;
    bool is_finished_ = false;

    uint32_t message_size_ = 0;
    ByteArray message_;

    asio::io_context& io_context_;
    asio::ip::tcp::socket socket_;
    asio::ip::tcp::resolver resolver_;

    DISALLOW_COPY_AND_ASSIGN(RelayPeer);
};

} // namespace base

#endif // BASE__PEER__RELAY_PEER_H
