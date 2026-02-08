//
// SmartCafe Project
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
#include "base/peer/server_authenticator.h"

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
void TcpServer::setUserList(SharedPointer<UserListBase> user_list)
{
    user_list_ = user_list;
    DCHECK(user_list_);
}

//--------------------------------------------------------------------------------------------------
void TcpServer::setPrivateKey(const QByteArray& private_key)
{
    private_key_ = private_key;
}

//--------------------------------------------------------------------------------------------------
void TcpServer::setAnonymousAccess(
    ServerAuthenticator::AnonymousAccess anonymous_access, quint32 session_types)
{
    anonymous_access_ = anonymous_access;
    anonymous_session_types_ = session_types;
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
bool TcpServer::hasReadyConnections()
{
    return !ready_.isEmpty();
}

//--------------------------------------------------------------------------------------------------
TcpChannel* TcpServer::nextReadyConnection()
{
    if (ready_.isEmpty())
        return nullptr;

    TcpChannel* channel = ready_.front();
    channel->setParent(nullptr);

    ready_.pop_front();
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

            ServerAuthenticator* authenticator = new ServerAuthenticator();
            authenticator->setUserList(user_list_);

            if (!private_key_.isEmpty())
            {
                if (!authenticator->setPrivateKey(private_key_))
                {
                    LOG(ERROR) << "Failed to set private key for authenticator";
                    delete authenticator;
                    return;
                }

                if (!authenticator->setAnonymousAccess(anonymous_access_, anonymous_session_types_))
                {
                    LOG(ERROR) << "Failed to set anonymous access settings";
                    delete authenticator;
                    return;
                }
            }

            TcpChannel* channel = new TcpChannel(std::move(socket), authenticator, this);

            connect(channel, &TcpChannel::sig_authenticated, this, [this, channel]()
            {
                removePendingChannel(channel);
                ready_.append(channel);
                emit sig_newConnection();
            });

            connect(channel, &TcpChannel::sig_errorOccurred,
                    this, [this, channel](TcpChannel::ErrorCode error_code)
            {
                removePendingChannel(channel);
                channel->deleteLater();
            });

            // Connection accepted.
            pending_.emplace_back(channel);

            // Start authentication.
            channel->doAuthentication();
        }

        // Accept next connection.
        doAccept();
    });
}

//--------------------------------------------------------------------------------------------------
void TcpServer::removePendingChannel(TcpChannel* channel)
{
    // Disconnect all connected signals between the server and channel.
    disconnect(channel, nullptr, this, nullptr);

    // Remove channel from pending queue.
    for (auto it = pending_.begin(); it != pending_.end(); ++it)
    {
        if (*it != channel)
            continue;

        pending_.erase(it);
        break;
    }
}

} // namespace base
