//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef NET__NETWORK_SERVER_H
#define NET__NETWORK_SERVER_H

#include "base/macros_magic.h"

#include <asio/ip/tcp.hpp>

namespace net {

class Channel;

class Server
{
public:
    explicit Server(asio::io_context& io_context);
    ~Server();

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onNewConnection(std::unique_ptr<Channel> channel) = 0;
    };

    void start(uint16_t port, Delegate* delegate);

private:
    void doAccept();

    asio::io_context& io_context_;
    std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
    Delegate* delegate_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(Server);
};

} // namespace net

#endif // NET__NETWORK_SERVER_H
