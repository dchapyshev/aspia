//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/net/tcp_server.h"

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_asio.h"
#include "base/net/tcp_channel.h"
#include "base/strings/unicode.h"

#include <asio/ip/address.hpp>

namespace base {

class TcpServer::Impl : public base::enable_shared_from_this<Impl>
{
public:
    explicit Impl(asio::io_context& io_context);
    ~Impl();

    void start(std::u16string_view listen_interface, uint16_t port, Delegate* delegate);
    void stop();

    std::u16string listenInterface() const;
    uint16_t port() const;

private:
    void doAccept();
    void onAccept(const std::error_code& error_code, asio::ip::tcp::socket socket);

    asio::io_context& io_context_;
    std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
    Delegate* delegate_ = nullptr;

    std::u16string listen_interface_;
    uint16_t port_ = 0;

    int accept_error_count_ = 0;

    DISALLOW_COPY_AND_ASSIGN(Impl);
};

//--------------------------------------------------------------------------------------------------
TcpServer::Impl::Impl(asio::io_context& io_context)
    : io_context_(io_context)
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
TcpServer::Impl::~Impl()
{
    LOG(LS_INFO) << "Dtor";
    DCHECK(!acceptor_);
}

//--------------------------------------------------------------------------------------------------
void TcpServer::Impl::start(std::u16string_view listen_interface, uint16_t port, Delegate* delegate)
{
    delegate_ = delegate;
    listen_interface_ = listen_interface;
    port_ = port;

    DCHECK(delegate_);

    asio::error_code error_code;
    asio::ip::address listen_address =
        asio::ip::make_address(base::local8BitFromUtf16(listen_interface), error_code);
    if (error_code)
    {
        LOG(LS_ERROR) << "Invalid listen address: " << listen_interface.data()
                      << " (" << base::utf16FromLocal8Bit(error_code.message()) << ")";
        return;
    }

    asio::ip::tcp::endpoint endpoint(listen_address, port);
    acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(io_context_);

    acceptor_->open(endpoint.protocol(), error_code);
    if (error_code)
    {
        LOG(LS_WARNING) << "acceptor_->open failed: "
                        << base::utf16FromLocal8Bit(error_code.message());
        return;
    }

    acceptor_->bind(endpoint, error_code);
    if (error_code)
    {
        LOG(LS_WARNING) << "acceptor_->bind failed: "
                        << base::utf16FromLocal8Bit(error_code.message());
    }

    acceptor_->listen(asio::ip::tcp::socket::max_listen_connections, error_code);
    if (error_code)
    {
        LOG(LS_WARNING) << "acceptor_->listen failed: "
                        << base::utf16FromLocal8Bit(error_code.message());
    }

    doAccept();
}

//--------------------------------------------------------------------------------------------------
void TcpServer::Impl::stop()
{
    delegate_ = nullptr;
    acceptor_.reset();
}

//--------------------------------------------------------------------------------------------------
std::u16string TcpServer::Impl::listenInterface() const
{
    return listen_interface_;
}

//--------------------------------------------------------------------------------------------------
uint16_t TcpServer::Impl::port() const
{
    return port_;
}

//--------------------------------------------------------------------------------------------------
void TcpServer::Impl::doAccept()
{
    acceptor_->async_accept(
        std::bind(&Impl::onAccept, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
}

//--------------------------------------------------------------------------------------------------
void TcpServer::Impl::onAccept(const std::error_code& error_code, asio::ip::tcp::socket socket)
{
    if (!delegate_)
        return;

    if (error_code)
    {
        LOG(LS_ERROR) << "Error while accepting connection: "
                      << base::utf16FromLocal8Bit(error_code.message());

        static const int kMaxErrorCount = 500;

        ++accept_error_count_;
        if (accept_error_count_ > kMaxErrorCount)
        {
            LOG(LS_ERROR) << "WARNING! Too many errors when trying to accept a connection. "
                          << "New connections will not be accepted";
            return;
        }
    }
    else
    {
        accept_error_count_ = 0;

        std::unique_ptr<TcpChannel> channel =
            std::unique_ptr<TcpChannel>(new TcpChannel(std::move(socket)));

        // Connection accepted.
        delegate_->onNewConnection(std::move(channel));
    }

    // Accept next connection.
    doAccept();
}

//--------------------------------------------------------------------------------------------------
TcpServer::TcpServer()
    : impl_(base::make_local_shared<Impl>(MessageLoop::current()->pumpAsio()->ioContext()))
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
TcpServer::~TcpServer()
{
    LOG(LS_INFO) << "Dtor";
    impl_->stop();
}

//--------------------------------------------------------------------------------------------------
void TcpServer::start(std::u16string_view listen_interface, uint16_t port, Delegate* delegate)
{
    impl_->start(listen_interface, port, delegate);
}

//--------------------------------------------------------------------------------------------------
void TcpServer::stop()
{
    impl_->stop();
}

//--------------------------------------------------------------------------------------------------
std::u16string TcpServer::listenInterface() const
{
    return impl_->listenInterface();
}

//--------------------------------------------------------------------------------------------------
uint16_t TcpServer::port() const
{
    return impl_->port();
}

//--------------------------------------------------------------------------------------------------
// static
bool TcpServer::isValidListenInterface(std::u16string_view interface)
{
    asio::error_code error_code;
    asio::ip::make_address_v4(base::local8BitFromUtf16(interface), error_code);
    if (error_code)
    {
        LOG(LS_WARNING) << "Invalid interface address: "
                        << base::utf16FromLocal8Bit(error_code.message());
        return false;
    }

    return true;
}

} // namespace base
