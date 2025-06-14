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
      acceptor_(AsioEventDispatcher::currentIoContext())
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
TcpServer::~TcpServer()
{
    LOG(INFO) << "Dtor";

    std::error_code ignored_error;
    acceptor_.cancel(ignored_error);
}

//--------------------------------------------------------------------------------------------------
void TcpServer::start(quint16 port, const QString& iface)
{
    if (acceptor_.is_open())
    {
        LOG(ERROR) << "Tcp server is already started";
        return;
    }

    LOG(INFO) << "Listen interface:" << (iface.isEmpty() ? "ANY" : iface) << ":" << port;

    asio::ip::address listen_address;
    asio::error_code error_code;

    if (!iface.isEmpty())
    {
        listen_address = asio::ip::make_address(iface.toLocal8Bit().toStdString(), error_code);
        if (error_code)
        {
            LOG(ERROR) << "Invalid listen address:" << iface << "(" << error_code << ")";
            return;
        }
    }
    else
    {
        listen_address = asio::ip::address_v6::any();
    }

    asio::ip::tcp::endpoint endpoint(listen_address, port);

    acceptor_.open(endpoint.protocol(), error_code);
    if (error_code)
    {
        LOG(ERROR) << "acceptor::open failed:" << error_code;
        return;
    }

    acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true), error_code);
    if (error_code)
    {
        LOG(ERROR) << "acceptor::set_option failed:" << error_code;
        return;
    }

    acceptor_.bind(endpoint, error_code);
    if (error_code)
    {
        LOG(ERROR) << "acceptor::bind failed:" << error_code;
        return;
    }

    acceptor_.listen(asio::ip::tcp::socket::max_listen_connections, error_code);
    if (error_code)
    {
        LOG(ERROR) << "acceptor::listen failed:" << error_code;
        return;
    }

    doAccept();
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
// static
bool TcpServer::isValidListenInterface(const QString& iface)
{
    if (iface.isEmpty())
        return true;

    asio::error_code error_code;
    asio::ip::make_address(iface.toLocal8Bit().toStdString(), error_code);
    if (error_code)
    {
        LOG(ERROR) << "Invalid interface address:" << error_code;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void TcpServer::doAccept()
{
    acceptor_.async_accept([this](const std::error_code& error_code, asio::ip::tcp::socket socket)
    {
        if (error_code)
        {
            if (error_code == asio::error::operation_aborted)
                return;

            LOG(ERROR) << "Error while accepting connection:" << error_code;

            static const int kMaxErrorCount = 50;

            ++accept_error_count_;
            if (accept_error_count_ > kMaxErrorCount)
            {
                LOG(ERROR) << "WARNING! Too many errors when trying to accept a connection. "
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
