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

#include "base/peer/stun_server.h"

#include "base/asio_event_dispatcher.h"
#include "base/logging.h"
#include "base/serialization.h"
#include "proto/stun.h"

namespace base {

//--------------------------------------------------------------------------------------------------
StunServer::StunServer(QObject* parent)
    : QObject(parent),
      udp_socket_(AsioEventDispatcher::ioContext())
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
StunServer::~StunServer()
{
    LOG(INFO) << "Dtor";

    // Mark guard before releasing resources so that any pending ASIO handlers
    // (already completed but not yet dispatched) will see the object is gone.
    *alive_guard_ = false;

    std::error_code ignored_error;
    udp_socket_.cancel(ignored_error);
    udp_socket_.close(ignored_error);
}

//--------------------------------------------------------------------------------------------------
bool StunServer::start(quint16 port)
{
    asio::ip::udp::endpoint endpoint(asio::ip::udp::v4(), port);
    port_ = port;

    std::error_code error_code;

    if (udp_socket_.is_open())
    {
        udp_socket_.cancel(error_code);
        udp_socket_.close(error_code);
        error_code.clear();
    }

    udp_socket_.open(endpoint.protocol(), error_code);
    if (error_code)
    {
        LOG(ERROR) << "Unable to open socket:" << error_code;
        return false;
    }

    udp_socket_.bind(endpoint, error_code);
    if (error_code)
    {
        LOG(ERROR) << "Unable to bind socket:" << error_code;

        std::error_code ignored_error;
        udp_socket_.close(ignored_error);
        return false;
    }

    doReceiveRequest();
    return true;
}

//--------------------------------------------------------------------------------------------------
void StunServer::doReceiveRequest()
{
    auto guard = alive_guard_;
    udp_socket_.async_receive_from(asio::buffer(read_buffer_.data(), read_buffer_.size()), remote_endpoint_,
        [this, guard](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (!*guard)
            return;

        if (error_code)
        {
            if (error_code == asio::error::operation_aborted)
                return;

            LOG(ERROR) << "Error reading from socket:" << error_code;
            doReceiveRequest();
            return;
        }

        proto::stun::PeerToStun message;
        if (!message.ParseFromArray(read_buffer_.data(), static_cast<int>(bytes_transferred)))
        {
            LOG(ERROR) << "Unable to parse message";
        }
        else if (message.has_endpoint_request())
        {
            const proto::stun::EndpointRequest& request = message.endpoint_request();

            if (request.magic_number() == 0xA0B1C2D3)
                doSendAddressReply(request.transaction_id(), remote_endpoint_);
            else
                LOG(ERROR) << "Invalid magic number:" << message.endpoint_request().magic_number();
        }

        doReceiveRequest();
    });
}

//--------------------------------------------------------------------------------------------------
bool StunServer::doSendAddressReply(quint32 transaction_id, const asio::ip::udp::endpoint& remote_endpoint)
{
    if (!udp_socket_.is_open())
    {
        LOG(ERROR) << "UDP socket is not open";
        return false;
    }

    proto::stun::StunToPeer message;
    proto::stun::Endpoint* endpoint = message.mutable_endpoint();

    endpoint->set_transaction_id(transaction_id);
    endpoint->set_ip_address(remote_endpoint.address().to_string());
    endpoint->set_port(remote_endpoint.port());

    QByteArray reply = base::serialize(message);
    if (reply.isEmpty())
    {
        LOG(ERROR) << "Unable to serialize message";
        return false;
    }

    udp_socket_.async_send_to(asio::buffer(reply.constData(), reply.size()), remote_endpoint,
        [reply](const std::error_code& error_code, size_t /* bytes_transferred */)
    {
        if (error_code)
        {
            if (error_code == asio::error::operation_aborted)
                return;
            LOG(ERROR) << "Error writing to socket:" << error_code;
        }
    });

    return true;
}

} // namespace base
