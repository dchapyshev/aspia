//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/net/tcp_server_legacy.h"

#include <asio/ip/address.hpp>

#include "base/asio_event_dispatcher.h"
#include "base/logging.h"
#include "base/net/flood_guard.h"
#include "base/net/net_utils.h"
#include "base/net/tcp_channel_legacy.h"
#include "base/peer/server_authenticator_legacy.h"

//--------------------------------------------------------------------------------------------------
TcpServerLegacy::TcpServerLegacy(QObject* parent)
    : QObject(parent),
      acceptor_(AsioEventDispatcher::ioContext()),
      flood_guard_(std::make_unique<FloodGuard>())
{
    // FloodGuard defaults (60/min per address, pending cap 32) suit a single-host listener.
    // Roles serving many peers (router) call setRateLimit / setMaxPendingConnections directly.
}

//--------------------------------------------------------------------------------------------------
TcpServerLegacy::~TcpServerLegacy()
{
    // Mark guard before releasing resources so that any pending ASIO handlers
    // (already completed but not yet dispatched) will see the object is gone.
    *alive_guard_ = false;

    std::error_code ignored_error;
    acceptor_.cancel(ignored_error);
}

//--------------------------------------------------------------------------------------------------
void TcpServerLegacy::setUserList(SharedPointer<UserList> user_list)
{
    user_list_ = user_list;
    DCHECK(user_list_);
}

//--------------------------------------------------------------------------------------------------
void TcpServerLegacy::setPrivateKey(const SecureByteArray& private_key)
{
    private_key_ = private_key;
}

//--------------------------------------------------------------------------------------------------
void TcpServerLegacy::setAnonymousAccess(
    ServerAuthenticatorLegacy::AnonymousAccess anonymous_access, quint32 session_types)
{
    anonymous_access_ = anonymous_access;
    anonymous_session_types_ = session_types;
}

//--------------------------------------------------------------------------------------------------
void TcpServerLegacy::setMaxPendingConnections(int max_pending)
{
    if (max_pending <= 0)
    {
        LOG(ERROR) << "Invalid max pending connections:" << max_pending;
        return;
    }

    flood_guard_->setMaxPending(max_pending);
}

//--------------------------------------------------------------------------------------------------
void TcpServerLegacy::setMaxConnectionsPerMinute(int max_per_minute)
{
    if (max_per_minute <= 0)
    {
        LOG(ERROR) << "Invalid max connections per minute:" << max_per_minute;
        return;
    }

    flood_guard_->setRateLimit(FloodGuard::Seconds{60}, max_per_minute);
}

//--------------------------------------------------------------------------------------------------
void TcpServerLegacy::setWhiteList(const QStringList& white_list)
{
    white_list_ = white_list;
}

//--------------------------------------------------------------------------------------------------
void TcpServerLegacy::start(quint16 port, const QString& iface)
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
bool TcpServerLegacy::hasReadyConnections()
{
    return !ready_.isEmpty();
}

//--------------------------------------------------------------------------------------------------
TcpChannel* TcpServerLegacy::nextReadyConnection()
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
bool TcpServerLegacy::isValidListenInterface(const QString& iface)
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
void TcpServerLegacy::doAccept()
{
    auto guard = alive_guard_;
    acceptor_.async_accept([this, guard](const std::error_code& error_code, asio::ip::tcp::socket socket)
    {
        if (!*guard)
            return;

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

            if (!flood_guard_->check(socket, pending_.size()))
            {
                // socket goes out of scope here - asio's destructor closes the descriptor.
                doAccept();
                return;
            }

            // The white list is enforced here, before the handshake, so unwanted peers are dropped
            // without consuming a pending slot or any cryptographic work.
            if (!white_list_.isEmpty())
            {
                std::error_code endpoint_error;
                const asio::ip::tcp::endpoint endpoint = socket.remote_endpoint(endpoint_error);
                const QString address = endpoint_error ?
                    QString() : QString::fromStdString(endpoint.address().to_string());

                if (!NetUtils::isAddressInWhiteList(white_list_, address))
                {
                    LOG(TRACE) << "Connection rejected by white list:" << address;
                    doAccept();
                    return;
                }
            }

            ServerAuthenticatorLegacy* authenticator = new ServerAuthenticatorLegacy();
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

            TcpChannel* channel = new TcpChannelLegacy(
                TcpChannel::Type::DIRECT, std::move(socket), authenticator, this);

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
void TcpServerLegacy::removePendingChannel(TcpChannel* channel)
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
