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

#include "base/net/tcp_channel_native.h"

#include <asio/connect.hpp>
#include <asio/ip/address.hpp>
#include <asio/read.hpp>
#include <asio/steady_timer.hpp>
#include <asio/write.hpp>

#include "base/location.h"
#include "base/threading/asio_event_dispatcher.h"

namespace {

//--------------------------------------------------------------------------------------------------
QStringList endpointsToString(const asio::ip::tcp::resolver::results_type& endpoints)
{
    if (endpoints.empty())
        return QStringList();

    QStringList list;
    list.reserve(endpoints.size());

    for (auto it = endpoints.begin(), it_end = endpoints.end(); it != it_end; ++it)
        list.emplace_back(QString::fromLocal8Bit(it->endpoint().address().to_string()));

    return list;
}

} // namespace

//--------------------------------------------------------------------------------------------------
TcpChannelNative::TcpChannelNative(Authenticator* authenticator, QObject* parent)
    : TcpChannel(Type::DIRECT, authenticator, parent),
      io_context_(AsioEventDispatcher::ioContext()),
      socket_(io_context_),
      resolver_(std::make_unique<asio::ip::tcp::resolver>(io_context_))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
TcpChannelNative::TcpChannelNative(
    Type type, asio::ip::tcp::socket&& socket, Authenticator* authenticator, QObject* parent)
    : TcpChannel(type, authenticator, parent),
      io_context_(AsioEventDispatcher::ioContext()),
      socket_(std::move(socket))
{
    CDCHECK(socket_.is_open());
    onConnected();
}

//--------------------------------------------------------------------------------------------------
TcpChannelNative::~TcpChannelNative()
{
    closeTransport();
}

//--------------------------------------------------------------------------------------------------
void TcpChannelNative::connectTo(const QString& address, quint16 port, Seconds timeout)
{
    if (isConnected() || !resolver_)
        return;

    std::string host = address.toLocal8Bit().toStdString();
    std::string service = std::to_string(port);

    // Watchdog for the entire connect phase (resolve + TCP handshake). Without it the channel
    // would wait for the OS-level SYN-retransmit timeout (~2 minutes on Linux) before reporting
    // an unreachable peer.
    SharedPointer<asio::steady_timer> watchdog(new asio::steady_timer(io_context_));
    watchdog->expires_after(timeout);

    watchdog->async_wait([this, io = ioState(), watchdog](const std::error_code& error_code)
    {
        if (io->alive && !error_code && !isConnected())
            onErrorOccurred(FROM_HERE, ErrorCode::SOCKET_TIMEOUT);
    });

    // Fast path for IP addresses. The ASIO resolver serializes all lookups for the io_context through
    // a single background thread, so a literal address could otherwise wait behind slow or failing
    // hostname lookups issued by other channels. Resolving is only needed for host names, so skip it
    // entirely here.
    std::error_code address_error;
    asio::ip::address ip_address = asio::ip::make_address(host, address_error);
    if (!address_error)
    {
        CLOG(TRACE) << "Address is an IP, skipping resolve for" << host << ":" << service;

        asio::ip::tcp::endpoint endpoint(ip_address, port);
        socket_.async_connect(endpoint, [this, io = ioState(), watchdog, endpoint](
            const std::error_code& error_code)
        {
            if (!io->alive)
                return;

            if (error_code)
            {
                if (error_code == asio::error::operation_aborted)
                    return;
                watchdog->cancel();
                onErrorOccurred(FROM_HERE, error_code);
                return;
            }

            CLOG(TRACE) << "Connected to:" << endpoint.address().to_string() << ":" << endpoint.port();
            watchdog->cancel();

            onConnected();
            emit sig_connected();

            doAuthentication();
        });
        return;
    }

    CLOG(TRACE) << "Start resolving for" << host << ":" << service;

    resolver_->async_resolve(host, service, [this, io = ioState(), watchdog](
        const std::error_code& error_code, const asio::ip::tcp::resolver::results_type& endpoints)
    {
        if (!io->alive)
            return;

        if (error_code)
        {
            if (error_code == asio::error::operation_aborted)
                return;
            watchdog->cancel();
            onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        CLOG(TRACE) << "Resolved:" << endpointsToString(endpoints);

        asio::async_connect(socket_, endpoints,
            [](const std::error_code& error_code, const asio::ip::tcp::endpoint& next)
        {
            if (error_code == asio::error::operation_aborted)
            {
                // If more than one address for a host was resolved, then we return false and cancel
                // attempts to connect to all addresses.
                return false;
            }

            return true;
        },
            [this, io = ioState(), watchdog](
                const std::error_code& error_code, const asio::ip::tcp::endpoint& endpoint)
        {
            if (!io->alive)
                return;

            if (error_code)
            {
                if (error_code == asio::error::operation_aborted)
                    return;
                watchdog->cancel();
                onErrorOccurred(FROM_HERE, error_code);
                return;
            }

            CLOG(TRACE) << "Connected to:" << endpoint.address().to_string() << ":" << endpoint.port();
            watchdog->cancel();

            onConnected();
            emit sig_connected();

            doAuthentication();
        });
    });
}

//--------------------------------------------------------------------------------------------------
bool TcpChannelNative::setReadBufferSize(int size)
{
    asio::socket_base::receive_buffer_size option(size);

    asio::error_code error_code;
    socket_.set_option(option, error_code);
    if (error_code)
    {
        CLOG(ERROR) << "Failed to set read buffer size:" << error_code;
        return false;
    }

    CLOG(INFO) << "Read buffer size is changed:" << size;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool TcpChannelNative::setWriteBufferSize(int size)
{
    asio::socket_base::send_buffer_size option(size);

    asio::error_code error_code;
    socket_.set_option(option, error_code);
    if (error_code)
    {
        CLOG(ERROR) << "Failed to set write buffer size:" << error_code;
        return false;
    }

    CLOG(INFO) << "Write buffer size is changed:" << size;
    return true;
}

//--------------------------------------------------------------------------------------------------
void TcpChannelNative::readTransport(void* data, size_t size)
{
    asio::async_read(socket_, asio::buffer(data, size),
        [this, io = ioState()](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (!io->alive)
            return;

        onReadCompleted(error_code, bytes_transferred);
    });
}

//--------------------------------------------------------------------------------------------------
void TcpChannelNative::writeTransport(const void* data, size_t size)
{
    asio::async_write(socket_, asio::buffer(data, size),
        [this, io = ioState()](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (!io->alive)
            return;

        onWriteCompleted(error_code, bytes_transferred);
    });
}

//--------------------------------------------------------------------------------------------------
void TcpChannelNative::closeTransport()
{
    ioState()->alive = false;

    if (resolver_)
    {
        resolver_->cancel();
        resolver_.reset();
    }

    if (socket_.is_open())
    {
        std::error_code ignored_code;
        socket_.cancel(ignored_code);
        socket_.close(ignored_code);
    }
}

//--------------------------------------------------------------------------------------------------
void TcpChannelNative::onConnected()
{
    std::string peer_address;
    asio::error_code error_code;

    asio::ip::tcp::no_delay no_delay_option(true);
    socket_.set_option(no_delay_option, error_code);
    if (!error_code)
        CLOG(TRACE) << "Nagle's algorithm is disabled";

    asio::socket_base::receive_buffer_size receive_option;
    socket_.get_option(receive_option, error_code);
    if (!error_code)
        CLOG(TRACE) << "Read buffer size:" << receive_option.value();

    asio::socket_base::send_buffer_size send_option;
    socket_.get_option(send_option, error_code);
    if (!error_code)
        CLOG(TRACE) << "Write buffer size:" << send_option.value();

    try
    {
        std::error_code error_code;
        asio::ip::tcp::endpoint endpoint = socket_.remote_endpoint(error_code);
        if (error_code)
        {
            CLOG(ERROR) << "Unable to get peer address:" << error_code;
        }
        else
        {
            asio::ip::address address = endpoint.address();
            if (address.is_v4())
            {
                asio::ip::address_v4 ipv4_address = address.to_v4();
                peer_address = ipv4_address.to_string();
            }
            else
            {
                asio::ip::address_v6 ipv6_address = address.to_v6();
                if (ipv6_address.is_v4_mapped())
                {
                    asio::ip::address_v4 ipv4_address =
                        asio::ip::make_address_v4(asio::ip::v4_mapped, ipv6_address);
                    peer_address = ipv4_address.to_string();
                }
                else
                {
                    peer_address = ipv6_address.to_string();
                }
            }
        }
    }
    catch (const std::error_code& error_code)
    {
        CLOG(ERROR) << "Unable to get peer address:" << error_code;
    }

    setConnected(peer_address);
}
