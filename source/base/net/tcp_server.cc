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

namespace base {

//--------------------------------------------------------------------------------------------------
TcpServer::TcpServer(QObject* parent)
    : QObject(parent),
      io_context_(AsioEventDispatcher::currentIoContext())
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
TcpServer::~TcpServer()
{
    LOG(LS_INFO) << "Dtor";
    stop();
}

//--------------------------------------------------------------------------------------------------
void TcpServer::start(const QString& listen_interface, quint16 port)
{
    listen_interface_ = listen_interface;
    port_ = port;

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
void TcpServer::stop()
{
    if (!acceptor_)
        return;

    std::error_code ignored_error;
    acceptor_->cancel(ignored_error);
    acceptor_.reset();
}

//--------------------------------------------------------------------------------------------------
bool TcpServer::hasPendingConnections()
{
    return !pending_.isEmpty();
}

//--------------------------------------------------------------------------------------------------
TcpChannel* TcpServer::nextPendingConnection()
{
    if (pending_.isEmpty())
        return nullptr;

    TcpChannel* channel = pending_.front();
    channel->setParent(nullptr);

    pending_.pop_front();
    return channel;
}

//--------------------------------------------------------------------------------------------------
QString TcpServer::listenInterface() const
{
    return listen_interface_;
}

//--------------------------------------------------------------------------------------------------
quint16 TcpServer::port() const
{
    return port_;
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
void TcpServer::doAccept()
{
    acceptor_->async_accept([this](const std::error_code& error_code, asio::ip::tcp::socket socket)
    {
        if (error_code)
        {
            if (error_code == asio::error::operation_aborted)
                return;

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

            // Connection accepted.
            pending_.push_back(new TcpChannel(std::move(socket), this));
            emit sig_newConnection();
        }

        // Accept next connection.
        doAccept();
    });
}

} // namespace base
