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
    std::error_code ignored_error;
    udp_socket_.cancel(ignored_error);
    udp_socket_.close(ignored_error);
}

//--------------------------------------------------------------------------------------------------
void StunServer::start(uint16_t port)
{
    asio::ip::udp::endpoint endpoint(asio::ip::udp::v4(), port);

    std::error_code error_code;
    udp_socket_.bind(endpoint, error_code);
    if (error_code)
    {
        LOG(ERROR) << "Unable to bind socket:" << error_code;
        return;
    }

    doReceiveRequest();
}

//--------------------------------------------------------------------------------------------------
void StunServer::doReceiveRequest()
{
    udp_socket_.async_receive(asio::buffer(buffer_.data(), buffer_.size()),
        [this](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (error_code)
        {
            if (error_code == asio::error::operation_aborted)
                return;
            LOG(ERROR) << "Error reading from socket:" << error_code;
        }
        else
        {
            proto::stun::PeerToStun message;
            if (!message.ParseFromArray(buffer_.data(), static_cast<int>(bytes_transferred)))
            {
                LOG(ERROR) << "Unable to parse message";
            }
            else
            {
                if (message.has_endpoint_request())
                {
                    if (message.endpoint_request().magic_number() == 0xA0B1C2D3)
                    {
                        if (doSendAddressReply())
                            return;
                    }
                    else
                    {
                        LOG(ERROR) << "Invalid magic number:" << message.endpoint_request().magic_number();
                    }
                }
            }
        }

        doReceiveRequest();
    });
}

//--------------------------------------------------------------------------------------------------
bool StunServer::doSendAddressReply()
{
    if (!udp_socket_.is_open())
    {
        LOG(ERROR) << "UDP socket is not open";
        return false;
    }

    std::error_code error_code;
    asio::ip::udp::endpoint remote_endpoint = udp_socket_.remote_endpoint(error_code);
    if (error_code)
    {
        LOG(ERROR) << "Unable to get remote endpoint from socket:" << error_code;
        return false;
    }

    proto::stun::StunToPeer message;
    proto::stun::Endpoint* endpoint = message.mutable_endpoint();
    endpoint->set_ip_address(remote_endpoint.address().to_string());
    endpoint->set_port(remote_endpoint.port());

    const size_t size = message.ByteSizeLong();
    if (!size || size > buffer_.size())
    {
        LOG(ERROR) << "Invalid message size:" << size;
        return false;
    }

    message.SerializeWithCachedSizesToArray(buffer_.data());

    udp_socket_.async_send(asio::buffer(buffer_.data(), size),
        [this](const std::error_code& error_code, size_t /* bytes_transferred */)
    {
        if (error_code)
        {
            if (error_code == asio::error::operation_aborted)
                return;
            LOG(ERROR) << "Error writing to socket:" << error_code;
        }

        doReceiveRequest();
    });

    return true;
}

} // namespace base
