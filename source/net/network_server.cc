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

#include "net/network_server.h"

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_asio.h"
#include "net/network_channel.h"

namespace net {

Server::Server()
    : io_context_(base::MessageLoop::current()->pumpAsio()->ioContext())
{
    // Nothing
}

Server::~Server() = default;

void Server::start(uint16_t port, Delegate* delegate)
{
    delegate_ = delegate;

    DCHECK(delegate_);

    asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), port);
    acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(io_context_, endpoint);

    doAccept();
}

void Server::doAccept()
{
    acceptor_->async_accept([this](const std::error_code& error_code, asio::ip::tcp::socket socket)
    {
        if (error_code)
            return;

        std::unique_ptr<Channel> channel =
            std::unique_ptr<Channel>(new Channel(std::move(socket)));

        // Connection accepted.
        delegate_->onNewConnection(std::move(channel));

        // Accept next connection.
        doAccept();
    });
}

} // namespace net
