//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/net/network_server.h"

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_asio.h"
#include "base/net/network_channel.h"
#include "base/strings/unicode.h"

namespace base {

class NetworkServer::Impl : public std::enable_shared_from_this<Impl>
{
public:
    explicit Impl(asio::io_context& io_context);
    ~Impl();

    void start(uint16_t port, Delegate* delegate);
    void stop();
    uint16_t port() const;

private:
    void doAccept();
    void onAccept(const std::error_code& error_code, asio::ip::tcp::socket socket);

    asio::io_context& io_context_;
    std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
    Delegate* delegate_ = nullptr;
    uint16_t port_ = 0;

    DISALLOW_COPY_AND_ASSIGN(Impl);
};

NetworkServer::Impl::Impl(asio::io_context& io_context)
    : io_context_(io_context)
{
    LOG(LS_INFO) << "Ctor";
}

NetworkServer::Impl::~Impl()
{
    LOG(LS_INFO) << "Dtor";
    DCHECK(!acceptor_);
}

void NetworkServer::Impl::start(uint16_t port, Delegate* delegate)
{
    delegate_ = delegate;
    port_ = port;

    DCHECK(delegate_);

    asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), port);
    acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(io_context_, endpoint);

    doAccept();
}

void NetworkServer::Impl::stop()
{
    delegate_ = nullptr;
    acceptor_.reset();
}

uint16_t NetworkServer::Impl::port() const
{
    return port_;
}

void NetworkServer::Impl::doAccept()
{
    acceptor_->async_accept(
        std::bind(&Impl::onAccept, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
}

void NetworkServer::Impl::onAccept(const std::error_code& error_code, asio::ip::tcp::socket socket)
{
    if (!delegate_)
        return;

    if (error_code)
    {
        LOG(LS_ERROR) << "Error while accepting connection: "
                      << base::utf16FromLocal8Bit(error_code.message());
    }
    else
    {
        std::unique_ptr<NetworkChannel> channel =
            std::unique_ptr<NetworkChannel>(new NetworkChannel(std::move(socket)));

        // Connection accepted.
        delegate_->onNewConnection(std::move(channel));
    }

    // Accept next connection.
    doAccept();
}

NetworkServer::NetworkServer()
    : impl_(std::make_shared<Impl>(MessageLoop::current()->pumpAsio()->ioContext()))
{
    LOG(LS_INFO) << "Ctor";
}

NetworkServer::~NetworkServer()
{
    LOG(LS_INFO) << "Dtor";
    impl_->stop();
}

void NetworkServer::start(uint16_t port, Delegate* delegate)
{
    impl_->start(port, delegate);
}

void NetworkServer::stop()
{
    impl_->stop();
}

uint16_t NetworkServer::port() const
{
    return impl_->port();
}

} // namespace base
