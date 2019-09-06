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

#include "net/socket_connector.h"

#include "base/strings/unicode.h"

#include <asio/connect.hpp>

namespace net {

SocketConnector::SocketConnector(asio::io_context& io_context)
    : resolver_(io_context)
{
    // Nothing
}

SocketConnector::~SocketConnector() = default;

void SocketConnector::attach(asio::ip::tcp::socket* socket,
                             ConnectCallback connect_callback,
                             ErrorCallback error_callback)
{
    socket_ = socket;
    connect_callback_ = std::move(connect_callback);
    error_callback_ = std::move(error_callback);
}

void SocketConnector::dettach()
{
    socket_ = nullptr;
}

void SocketConnector::connect(std::u16string_view address, uint16_t port)
{
    resolver_.async_resolve(base::local8BitFromUtf16(address), std::to_string(port),
                            std::bind(&SocketConnector::onResolved,
                                      shared_from_this(),
                                      std::placeholders::_1,
                                      std::placeholders::_2));
}

void SocketConnector::onResolved(const std::error_code& error_code,
                                 const asio::ip::tcp::resolver::results_type& endpoints)
{
    if (!socket_)
        return;

    if (error_code)
    {
        error_callback_(error_code);
        return;
    }

    asio::async_connect(*socket_, endpoints,
                        std::bind(&SocketConnector::onConnected,
                                  shared_from_this(),
                                  std::placeholders::_1,
                                  std::placeholders::_2));
}

void SocketConnector::onConnected(const std::error_code& error_code,
                                  const asio::ip::tcp::endpoint& /* endpoint */)
{
    if (!socket_)
        return;

    if (error_code)
    {
        error_callback_(error_code);
        return;
    }

    connect_callback_();
}

} // namespace net
