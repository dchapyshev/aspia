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

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_asio.h"
#include "proto/stun_peer.pb.h"
#include "base/strings/unicode.h"

namespace base {

StunServer::StunServer()
    : udp_socket_(MessageLoop::current()->pumpAsio()->ioContext())
{
    LOG(LS_INFO) << "Ctor";
}

StunServer::~StunServer()
{
    LOG(LS_INFO) << "Dtor";

    std::error_code ignored_error;
    udp_socket_.cancel(ignored_error);
    udp_socket_.close(ignored_error);
}

void StunServer::start(uint16_t port)
{
    asio::ip::udp::endpoint endpoint(asio::ip::udp::v4(), port);

    std::error_code error_code;
    udp_socket_.bind(endpoint, error_code);
    if (error_code)
    {
        LOG(LS_ERROR) << "Unable to bind socket: "
                      << base::utf16FromLocal8Bit(error_code.message());
        return;
    }

    doReceiveRequest();
}

void StunServer::doReceiveRequest()
{
    udp_socket_.async_receive(asio::buffer(buffer_.data(), buffer_.size()),
        [this](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (error_code)
        {
            if (error_code == asio::error::operation_aborted)
                return;

            LOG(LS_ERROR) << "Error reading from socket: "
                          << base::utf16FromLocal8Bit(error_code.message());
        }
        else
        {
            proto::PeerToStun message;
            if (!message.ParseFromArray(buffer_.data(), static_cast<int>(bytes_transferred)))
            {
                LOG(LS_ERROR) << "Unable to parse message";
            }
            else
            {
                if (message.has_external_endpoint_request())
                {
                    if (message.external_endpoint_request().magic_number() == 0xA0B1C2D3)
                    {
                        if (doSendAddressReply())
                            return;
                    }
                    else
                    {
                        LOG(LS_ERROR) << "Invalid magic number: "
                                      << message.external_endpoint_request().magic_number();
                    }
                }
            }
        }

        doReceiveRequest();
    });
}

bool StunServer::doSendAddressReply()
{
    if (!udp_socket_.is_open())
    {
        LOG(LS_ERROR) << "UDP socket not open";
        return false;
    }

    std::error_code error_code;
    asio::ip::udp::endpoint remote_endpoint = udp_socket_.remote_endpoint(error_code);
    if (error_code)
    {
        LOG(LS_ERROR) << "Unable to get remote endpoint from socket: "
                      << base::utf16FromLocal8Bit(error_code.message());
        return false;
    }

    proto::StunToPeer message;
    proto::ExternalEndpoint* external_endpoint = message.mutable_external_endpoint();
    external_endpoint->set_host(remote_endpoint.address());
    external_endpoint->set_port(remote_endpoint.port());

    const size_t size = message.ByteSizeLong();
    if (!size || size > buffer_.size())
    {
        LOG(LS_ERROR) << "Invalid message size: " << size;
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

            LOG(LS_ERROR) << "Error writing to socket: "
                          << base::utf16FromLocal8Bit(error_code.message());
        }

        doReceiveRequest();
    });

    return true;
}

} // namespace base
