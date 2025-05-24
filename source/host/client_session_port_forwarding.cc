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

#include "host/client_session_port_forwarding.h"

#include "base/asio_event_dispatcher.h"
#include "base/logging.h"
#include "base/serialization.h"

#include <asio/connect.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>

namespace host {

namespace {

QString endpointsToString(const asio::ip::tcp::resolver::results_type& endpoints)
{
    QString str;

    for (auto it = endpoints.begin(); it != endpoints.end();)
    {
        str += QString::fromStdString(it->endpoint().address().to_string());
        if (++it != endpoints.end())
            str += ", ";
    }

    return str;
}

} // namespace

class ClientSessionPortForwarding::Handler
{
public:
    Handler(ClientSessionPortForwarding* parent);
    ~Handler();

    void dettach();

    void onResolved(const std::error_code& error_code,
                    const asio::ip::tcp::resolver::results_type& endpoints);
    void onConnected(const std::error_code& error_code, const asio::ip::tcp::endpoint& endpoint);
    void onWrite(const std::error_code& error_code, size_t bytes_transferred);
    void onRead(const std::error_code& error_code, size_t bytes_transferred);

private:
    ClientSessionPortForwarding* parent_;

    DISALLOW_COPY_AND_ASSIGN(Handler);
};

//--------------------------------------------------------------------------------------------------
ClientSessionPortForwarding::Handler::Handler(ClientSessionPortForwarding* parent)
    : parent_(parent)
{
    DCHECK(parent_);
}

//--------------------------------------------------------------------------------------------------
ClientSessionPortForwarding::Handler::~Handler() = default;

//--------------------------------------------------------------------------------------------------
void ClientSessionPortForwarding::Handler::dettach()
{
    parent_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void ClientSessionPortForwarding::Handler::onResolved(
    const std::error_code& error_code,
    const asio::ip::tcp::resolver::results_type& endpoints)
{
    if (parent_)
        parent_->onResolved(error_code, endpoints);
}

//--------------------------------------------------------------------------------------------------
void ClientSessionPortForwarding::Handler::onConnected(
    const std::error_code& error_code, const asio::ip::tcp::endpoint& endpoint)
{
    if (parent_)
        parent_->onConnected(error_code, endpoint);
}

//--------------------------------------------------------------------------------------------------
void ClientSessionPortForwarding::Handler::onWrite(
    const std::error_code& error_code, size_t bytes_transferred)
{
    if (parent_)
        parent_->onWrite(error_code, bytes_transferred);
}

//--------------------------------------------------------------------------------------------------
void ClientSessionPortForwarding::Handler::onRead(
    const std::error_code& error_code, size_t bytes_transferred)
{
    if (parent_)
        parent_->onRead(error_code, bytes_transferred);
}

//--------------------------------------------------------------------------------------------------
ClientSessionPortForwarding::ClientSessionPortForwarding(
    base::TcpChannel* channel, QObject* parent)
    : ClientSession(proto::SESSION_TYPE_PORT_FORWARDING, channel, parent),
      socket_(base::AsioEventDispatcher::currentIoContext()),
      resolver_(base::AsioEventDispatcher::currentIoContext()),
      handler_(new Handler(this))
{
    LOG(LS_INFO) << "Ctor";
};

//--------------------------------------------------------------------------------------------------
ClientSessionPortForwarding::~ClientSessionPortForwarding()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ClientSessionPortForwarding::onStarted()
{
    LOG(LS_INFO) << "Session started";
}

//--------------------------------------------------------------------------------------------------
void ClientSessionPortForwarding::onReceived(const QByteArray& buffer)
{
    incoming_message_.Clear();

    if (!base::parse(buffer, &incoming_message_))
    {
        LOG(LS_ERROR) << "Unable to parse message";
        onError(FROM_HERE);
        return;
    }

    if (incoming_message_.has_data())
    {
        if (!is_started_)
        {
            LOG(LS_ERROR) << "Data received, but port forwarding is not started";
            onError(FROM_HERE);
            return;
        }

        if (state_ != State::CONNECTED)
        {
            LOG(LS_ERROR) << "Data received without connected socket";
            onError(FROM_HERE);
            return;
        }

        proto::port_forwarding::Data* data = incoming_message_.mutable_data();
        const bool schedule_write = write_queue_.empty();

        write_queue_.emplace(std::move(*data->mutable_data()));

        if (schedule_write)
            doWrite();
    }
    else if (incoming_message_.has_request())
    {
        const proto::port_forwarding::Request& request = incoming_message_.request();

        LOG(LS_INFO) << "Port forwarding request received (host: " << request.remote_host()
                     << " port: " << request.remote_port() << ")";

        if (request.remote_port() == 0 || request.remote_port() > 65535)
        {
            LOG(LS_ERROR) << "Port value is invalid. Connection will be closed";
            onError(FROM_HERE);
            return;
        }

        std::string host = request.remote_host();
        std::string service = std::to_string(request.remote_port());

        if (host.empty())
            host = "127.0.0.1";

        LOG(LS_INFO) << "Start resolving for " << host << ":" << service;
        state_ = State::RESOLVING;

        resolver_.async_resolve(host, service,
                                std::bind(&Handler::onResolved,
                                          handler_,
                                          std::placeholders::_1,
                                          std::placeholders::_2));
    }
    else if (incoming_message_.has_start())
    {
        LOG(LS_INFO) << "Port forwarding start received";

        if (!forwarding_result_sended_)
        {
            LOG(LS_ERROR) << "Start received but forwarding result not sended yet";
            onError(FROM_HERE);
            return;
        }

        is_started_ = true;
        doRead();
    }
    else
    {
        LOG(LS_ERROR) << "Unhandled message";
    }
}

//--------------------------------------------------------------------------------------------------
void ClientSessionPortForwarding::onError(const base::Location& location)
{
    LOG(LS_ERROR) << "Error occurred (from: " << location.toString() << ")";

    state_ = State::DISCONNECTED;
    if (handler_)
        handler_->dettach();

    resolver_.cancel();

    if (socket_.is_open())
    {
        LOG(LS_INFO) << "Cancel async operations";
        std::error_code ignored_code;
        socket_.cancel(ignored_code);

        LOG(LS_INFO) << "Close socket";
        socket_.close(ignored_code);
    }
    else
    {
        LOG(LS_INFO) << "Socket already closed";
    }

    stop();
}

//--------------------------------------------------------------------------------------------------
void ClientSessionPortForwarding::onResolved(
    const std::error_code& error_code,
    const asio::ip::tcp::resolver::results_type& endpoints)
{
    if (error_code)
    {
        LOG(LS_ERROR) << "Unable to resolve: " << error_code;
        onError(FROM_HERE);
        return;
    }

    LOG(LS_INFO) << "Resolved endpoints: " << endpointsToString(endpoints);
    state_ = State::CONNECTING;

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
    std::bind(&Handler::onConnected, handler_, std::placeholders::_1, std::placeholders::_2));
}

//--------------------------------------------------------------------------------------------------
void ClientSessionPortForwarding::onConnected(
    const std::error_code& error_code, const asio::ip::tcp::endpoint& endpoint)
{
    if (error_code)
    {
        LOG(LS_ERROR) << "Unable to connect: " << error_code;
        onError(FROM_HERE);
        return;
    }

    LOG(LS_INFO) << "Connected to endpoint: " << endpoint.address().to_string()
                 << ":" << endpoint.port();
    state_ = State::CONNECTED;

    sendPortForwardingResult(proto::port_forwarding::Result::SUCCESS);
}

//--------------------------------------------------------------------------------------------------
void ClientSessionPortForwarding::onWrite(
    const std::error_code& error_code, size_t bytes_transferred)
{
    if (error_code)
    {
        LOG(LS_ERROR) << "Unable to write: " << error_code;
        onError(FROM_HERE);
        return;
    }

    // Delete the sent message from the queue.
    if (!write_queue_.empty())
        write_queue_.pop();

    // If the queue is not empty, then we send the following message.
    if (!write_queue_.empty())
        doWrite();
}

//--------------------------------------------------------------------------------------------------
void ClientSessionPortForwarding::onRead(
    const std::error_code& error_code, size_t bytes_transferred)
{
    if (error_code)
    {
        LOG(LS_ERROR) << "Unable to read: " << error_code;
        onError(FROM_HERE);
        return;
    }

    sendPortForwardingData(read_buffer_.data(), bytes_transferred);

    // Read next data from socket.
    doRead();
}

//--------------------------------------------------------------------------------------------------
void ClientSessionPortForwarding::doWrite()
{
    const std::string& task = write_queue_.front();

    if (task.empty())
    {
        onError(FROM_HERE);
        return;
    }

    // Send the buffer to the recipient.
    asio::async_write(socket_,
                      asio::buffer(task.data(), task.size()),
                      std::bind(&Handler::onWrite,
                                handler_,
                                std::placeholders::_1,
                                std::placeholders::_2));
}

//--------------------------------------------------------------------------------------------------
void ClientSessionPortForwarding::doRead()
{
    socket_.async_read_some(
        asio::buffer(read_buffer_.data(), read_buffer_.size()),
        std::bind(&Handler::onRead, handler_, std::placeholders::_1, std::placeholders::_2));
}

//--------------------------------------------------------------------------------------------------
void ClientSessionPortForwarding::sendPortForwardingResult(
    proto::port_forwarding::Result::ErrorCode error_code)
{
    forwarding_result_sended_ = true;
    outgoing_message_.Clear();

    proto::port_forwarding::Result* result = outgoing_message_.mutable_result();
    result->set_error_code(error_code);

    LOG(LS_INFO) << "Sending port forwarding result: " << error_code;
    sendMessage(outgoing_message_);
}

void ClientSessionPortForwarding::sendPortForwardingData(const char *buffer, size_t length)
{
    outgoing_message_.Clear();

    proto::port_forwarding::Data* data = outgoing_message_.mutable_data();
    data->set_data(buffer, length);

    // Send data to client.
    sendMessage(outgoing_message_);
}

} // namespace host
