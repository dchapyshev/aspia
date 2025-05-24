//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/asio_event_dispatcher.h"
#include "base/logging.h"
#include "base/net/tcp_channel.h"

#include <asio/ip/address.hpp>

namespace base {

class TcpServer::Impl : public std::enable_shared_from_this<Impl>
{
public:
    explicit Impl(asio::io_context& io_context);
    ~Impl();

    void start(const QString& listen_interface, quint16 port, TcpServer* server);
    void stop();

    QString listenInterface() const;
    quint16 port() const;

private:
    void doAccept();
    void onAccept(const std::error_code& error_code, asio::ip::tcp::socket socket);

    asio::io_context& io_context_;
    std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
    TcpServer* server_ = nullptr;

    QString listen_interface_;
    quint16 port_ = 0;

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
void TcpServer::Impl::start(const QString& listen_interface, quint16 port, TcpServer* server)
{
    server_ = server;
    listen_interface_ = listen_interface;
    port_ = port;

    DCHECK(server_);

    LOG(LS_INFO) << "Listen interface: "
                 << (listen_interface_.isEmpty() ? "ANY" : listen_interface_) << ":" << port;

    asio::ip::address listen_address;
    asio::error_code error_code;

    if (!listen_interface_.isEmpty())
    {
        listen_address = asio::ip::make_address(
            listen_interface.toLocal8Bit().toStdString(), error_code);
        if (error_code)
        {
            LOG(LS_ERROR) << "Invalid listen address: " << listen_interface_
                          << " (" << error_code << ")";
            return;
        }
    }
    else
    {
        listen_address = asio::ip::address_v6::any();
    }

    asio::ip::tcp::endpoint endpoint(listen_address, port_);
    acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(io_context_);

    acceptor_->open(endpoint.protocol(), error_code);
    if (error_code)
    {
        LOG(LS_ERROR) << "acceptor_->open failed: " << error_code;
        return;
    }

    acceptor_->set_option(asio::ip::tcp::acceptor::reuse_address(true), error_code);
    if (error_code)
    {
        LOG(LS_ERROR) << "acceptor_->set_option failed: " << error_code;
        return;
    }

    acceptor_->bind(endpoint, error_code);
    if (error_code)
    {
        LOG(LS_ERROR) << "acceptor_->bind failed: " << error_code;
        return;
    }

    acceptor_->listen(asio::ip::tcp::socket::max_listen_connections, error_code);
    if (error_code)
    {
        LOG(LS_ERROR) << "acceptor_->listen failed: " << error_code;
        return;
    }

    doAccept();
}

//--------------------------------------------------------------------------------------------------
void TcpServer::Impl::stop()
{
    server_ = nullptr;
    acceptor_.reset();
}

//--------------------------------------------------------------------------------------------------
QString TcpServer::Impl::listenInterface() const
{
    return listen_interface_;
}

//--------------------------------------------------------------------------------------------------
quint16 TcpServer::Impl::port() const
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
    if (!server_)
        return;

    if (error_code)
    {
        LOG(LS_ERROR) << "Error while accepting connection: " << error_code;

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
            std::unique_ptr<TcpChannel>(new TcpChannel(std::move(socket), nullptr));

        // Connection accepted.
        server_->onNewConnection(std::move(channel));
    }

    // Accept next connection.
    doAccept();
}

//--------------------------------------------------------------------------------------------------
TcpServer::TcpServer(QObject* parent)
    : QObject(parent),
      impl_(std::make_shared<Impl>(AsioEventDispatcher::currentIoContext()))
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
void TcpServer::start(const QString& listen_interface, quint16 port)
{
    impl_->start(listen_interface, port, this);
}

//--------------------------------------------------------------------------------------------------
void TcpServer::stop()
{
    impl_->stop();
}

//--------------------------------------------------------------------------------------------------
bool TcpServer::hasPendingConnections()
{
    return !pending_.empty();
}

//--------------------------------------------------------------------------------------------------
TcpChannel* TcpServer::nextPendingConnection()
{
    if (pending_.empty())
        return nullptr;

    TcpChannel* channel = pending_.front().release();
    pending_.pop();

    return channel;
}

//--------------------------------------------------------------------------------------------------
QString TcpServer::listenInterface() const
{
    return impl_->listenInterface();
}

//--------------------------------------------------------------------------------------------------
quint16 TcpServer::port() const
{
    return impl_->port();
}

//--------------------------------------------------------------------------------------------------
// static
bool TcpServer::isValidListenInterface(const QString& iface)
{
    if (iface.isEmpty())
        return true;

    asio::error_code error_code;
    asio::ip::make_address(iface.toLocal8Bit().toStdString(), error_code);
    if (error_code)
    {
        LOG(LS_ERROR) << "Invalid interface address: " << error_code;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void TcpServer::onNewConnection(std::unique_ptr<TcpChannel> channel)
{
    pending_.emplace(std::move(channel));
    emit sig_newConnection();
}

} // namespace base
