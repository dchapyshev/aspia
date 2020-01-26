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

#ifndef NET__SOCKET_CONNECTOR_H
#define NET__SOCKET_CONNECTOR_H

#include "base/macros_magic.h"

#include <asio/ip/tcp.hpp>

#include <functional>

namespace net {

class SocketConnector
{
public:
    using ConnectCallback = std::function<void()>;
    using ErrorCallback = std::function<void(const std::error_code& error_code)>;

    SocketConnector(asio::io_context& io_context);
    ~SocketConnector();

    void attach(asio::ip::tcp::socket* socket,
                ConnectCallback connect_callback,
                ErrorCallback error_callback);
    void dettach();

    void connect(std::u16string_view address, uint16_t port);

private:
    void onResolved(const std::error_code& error_code,
                    const asio::ip::tcp::resolver::results_type& endpoints);
    void onConnected(const std::error_code& error_code, const asio::ip::tcp::endpoint& endpoint);

    asio::ip::tcp::socket* socket_ = nullptr;
    asio::ip::tcp::resolver resolver_;

    ConnectCallback connect_callback_;
    ErrorCallback error_callback_;

    DISALLOW_COPY_AND_ASSIGN(SocketConnector);
};

} // namespace net

#endif // NET__SOCKET_CONNECTOR_H
