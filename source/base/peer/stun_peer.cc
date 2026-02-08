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

#include "base/peer/stun_peer.h"

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_asio.h"
#include "base/strings/unicode.h"

#include <asio/connect.hpp>

namespace base {

namespace {

asio::io_context& ioContext()
{
    return MessageLoop::current()->pumpAsio()->ioContext();
}

} // namespace

StunPeer::StunPeer()
    : timer_(base::WaitableTimer::Type::SINGLE_SHOT, base::MessageLoop::current()->taskRunner()),
      udp_resolver_(ioContext()),
      udp_socket_(ioContext())
{
    LOG(LS_INFO) << "Ctor";
}

StunPeer::~StunPeer()
{
    LOG(LS_INFO) << "Dtor";

    delegate_ = nullptr;
    doStop();
}

void StunPeer::start(std::u16string_view stun_host, uint16_t stun_port, Delegate* delegate)
{
    delegate_ = delegate;
    DCHECK(delegate_);

    stun_host_ = local8BitFromUtf16(stun_host);
    stun_port_ = std::to_string(stun_port);

    doStart();
}

void StunPeer::doStart()
{
    ++number_of_attempts_;

    LOG(LS_INFO) << "Stun peer starting (server: " << stun_host_ << ":" << stun_port_
                 << ", attempt #" << number_of_attempts_ << ")";

    timer_.start(std::chrono::seconds(5), [this]()
    {
        static const int kMaxAttemptsCount = 3;

        if (number_of_attempts_ > kMaxAttemptsCount)
        {
            onErrorOccurred(FROM_HERE, std::error_code());
            return;
        }

        doStop();
        doStart();
    });

    udp_resolver_.async_resolve(stun_host_, stun_port_,
        [=](const std::error_code& error_code,
            const asio::ip::udp::resolver::results_type& endpoints)
    {
        if (error_code)
        {
            if (error_code != asio::error::operation_aborted)
                onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        LOG(LS_INFO) << "Resolved endpoints:";
        for (const auto& it : endpoints)
            LOG(LS_INFO) << it.endpoint().address() << ":" << it.endpoint().port();

        asio::async_connect(udp_socket_, endpoints,
            [=](const std::error_code& error_code, const asio::ip::udp::endpoint& endpoint)
        {
            if (error_code)
            {
                if (error_code != asio::error::operation_aborted)
                    onErrorOccurred(FROM_HERE, error_code);
                return;
            }

            LOG(LS_INFO) << "Connected to " << endpoint.address() << ":" << endpoint.port();
            doReceiveExternalAddress();
        });
    });
}

void StunPeer::doStop()
{
    timer_.stop();
    udp_resolver_.cancel();

    std::error_code ignored_code;
    udp_socket_.cancel(ignored_code);
    udp_socket_.close(ignored_code);
}

void StunPeer::doReceiveExternalAddress()
{
    proto::PeerToStun message;
    message.mutable_external_endpoint_request()->set_magic_number(0xA0B1C2D3);

    const size_t size = message.ByteSizeLong();
    if (!size || size > buffer_.size())
    {
        LOG(LS_ERROR) << "Invalid message size: " << size;
        onErrorOccurred(FROM_HERE, std::error_code());
        return;
    }

    message.SerializeWithCachedSizesToArray(buffer_.data());

    udp_socket_.async_send(asio::buffer(buffer_.data(), size),
        [this](const std::error_code& error_code, size_t /* bytes_transferred */)
    {
        if (error_code)
        {
            if (error_code != asio::error::operation_aborted)
                onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        udp_socket_.async_receive(asio::buffer(buffer_.data(), buffer_.size()),
            [this](const std::error_code& error_code, size_t bytes_transferred)
        {
            if (error_code != asio::error::operation_aborted)
            {
                onErrorOccurred(FROM_HERE, error_code);
                return;
            }

            proto::StunToPeer message;
            if (!message.ParseFromArray(buffer_.data(), static_cast<int>(bytes_transferred)))
            {
                onErrorOccurred(FROM_HERE, error_code);
                return;
            }

            if (!message.has_external_endpoint())
            {
                onErrorOccurred(FROM_HERE, error_code);
                return;
            }

            const proto::ExternalEndpoint& external_endpoint = message.external_endpoint();
            const std::string& host = external_endpoint.host();
            const uint16_t port = static_cast<uint16_t>(external_endpoint.port());

            LOG(LS_INFO) << "External endpoint: " << host << ":" << port;

            if (host.empty() || !port)
            {
                onErrorOccurred(FROM_HERE, error_code);
                return;
            }

            std::error_code error;
            asio::ip::address address = asio::ip::make_address(host, error);
            if (error)
            {
                onErrorOccurred(FROM_HERE, error);
                return;
            }

            if (delegate_)
            {
                delegate_->onStunExternalEndpoint(std::move(udp_socket_),
                    asio::ip::udp::endpoint(address, port));
                delegate_ = nullptr;
            }
        });
    });
}

void StunPeer::onErrorOccurred(const base::Location& location, const std::error_code& error_code)
{
    if (error_code)
    {
        LOG(LS_ERROR) << "Error occurred: " << base::utf16FromLocal8Bit(error_code.message())
                      << " (from: " << location.toString() << ")";
    }
    else
    {
        LOG(LS_ERROR) << "Error occurred (from: " << location.toString() << ")";
    }

    if (delegate_)
    {
        delegate_->onStunErrorOccurred();
        delegate_ = nullptr;
    }
}

} // namespace base
