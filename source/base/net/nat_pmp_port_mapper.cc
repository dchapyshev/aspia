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

#include "base/net/nat_pmp_port_mapper.h"

#include <QHostAddress>
#include <QtEndian>

#include <chrono>

#include "base/asio_event_dispatcher.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/net/net_utils.h"

namespace {

const quint16 kNatPmpPort = 5351;
const quint8 kNatPmpVersion = 0;
const quint8 kResponseFlag = 128; // Responses echo the request opcode with the high bit set.
const quint32 kLeaseSeconds = 3600; // Renewed on each connection attempt.

enum OpCode : quint8
{
    PUBLIC_ADDRESS = 0,
    MAP_UDP = 1
};

// NAT-PMP retransmits with an exponential backoff starting at 250 ms. A supporting gateway answers
// on the first probe, so a small cap keeps the fallback to UPnP fast when there is none.
const int kInitialTimeoutMs = 250;
const int kMaxTries = 4;

} // namespace

//--------------------------------------------------------------------------------------------------
NatPmpPortMapper::NatPmpPortMapper(QObject* parent)
    : QObject(parent),
      io_context_(AsioEventDispatcher::ioContext()),
      socket_(io_context_),
      retransmit_timer_(io_context_)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
NatPmpPortMapper::~NatPmpPortMapper()
{
    // Neutralize any in-flight async handlers: they hold a shared copy of this flag and bail out
    // instead of touching the destroyed object.
    *alive_guard_ = false;

    if (!mapped_)
        return;

    // Best-effort removal: a request with suggested external port 0 and lifetime 0 drops the
    // mapping. A single datagram is sent synchronously; the response is not awaited.
    MappingRequest request = {};
    request.version = kNatPmpVersion;
    request.opcode = OpCode::MAP_UDP;
    request.internal_port = internal_port_;

    std::error_code error_code;
    socket_.send_to(asio::buffer(&request, sizeof(request)), gateway_, 0, error_code);
}

//--------------------------------------------------------------------------------------------------
void NatPmpPortMapper::addUdpMapping(quint16 internal_port)
{
    internal_port_ = internal_port;

    const QString gateway = NetUtils::defaultGatewayAddress();
    if (gateway.isEmpty())
    {
        LOG(INFO) << "No default gateway found for NAT-PMP";
        finishWithFailure(FROM_HERE);
        return;
    }

    std::error_code error_code;
    asio::ip::address address = asio::ip::make_address(gateway.toStdString(), error_code);
    if (error_code || !address.is_v4())
    {
        finishWithFailure(FROM_HERE);
        return;
    }

    gateway_ = asio::ip::udp::endpoint(address, kNatPmpPort);

    socket_.open(asio::ip::udp::v4(), error_code);
    if (error_code)
    {
        LOG(WARNING) << "Unable to open NAT-PMP socket:" << error_code.message();
        finishWithFailure(FROM_HERE);
        return;
    }

    startPhase(Phase::PUBLIC_ADDRESS);
}

//--------------------------------------------------------------------------------------------------
void NatPmpPortMapper::startPhase(Phase phase)
{
    phase_ = phase;
    tries_ = 0;

    if (phase == Phase::PUBLIC_ADDRESS)
    {
        PublicAddressRequest request = {};
        request.version = kNatPmpVersion;
        request.opcode = OpCode::PUBLIC_ADDRESS;
        request_ = request;
    }
    else
    {
        MappingRequest request = {};
        request.version = kNatPmpVersion;
        request.opcode = OpCode::MAP_UDP;
        request.internal_port = internal_port_;
        request.external_port = internal_port_;
        request.lifetime = kLeaseSeconds;
        request_ = request;
    }

    startReceive();
    sendRequest();
}

//--------------------------------------------------------------------------------------------------
void NatPmpPortMapper::sendRequest()
{
    auto guard = alive_guard_;

    // The variant carries the active request type, so its wire size is known statically here.
    std::visit([this, guard](const auto& request)
    {
        socket_.async_send_to(asio::buffer(&request, sizeof(request)), gateway_,
            [this, guard](const std::error_code& error_code, size_t /* bytes */)
        {
            if (!*guard || finished_)
                return;

            if (error_code)
            {
                finishWithFailure(FROM_HERE);
                return;
            }

            startRetransmitTimer();
        });
    }, request_);
}

//--------------------------------------------------------------------------------------------------
void NatPmpPortMapper::startRetransmitTimer()
{
    retransmit_timer_.expires_after(std::chrono::milliseconds(kInitialTimeoutMs << tries_));

    auto guard = alive_guard_;
    retransmit_timer_.async_wait([this, guard](const std::error_code& error_code)
    {
        // A cancelled timer (response arrived) reports operation_aborted.
        if (!*guard || finished_ || error_code)
            return;

        if (++tries_ >= kMaxTries)
        {
            finishWithFailure(FROM_HERE);
            return;
        }

        sendRequest();
    });
}

//--------------------------------------------------------------------------------------------------
void NatPmpPortMapper::startReceive()
{
    auto guard = alive_guard_;
    socket_.async_receive_from(asio::buffer(&response_, sizeof(response_)), sender_,
        [this, guard](const std::error_code& error_code, size_t bytes)
    {
        if (!*guard)
            return;

        onResponse(error_code, bytes);
    });
}

//--------------------------------------------------------------------------------------------------
void NatPmpPortMapper::onResponse(const std::error_code& error_code, size_t bytes)
{
    if (finished_)
        return;

    if (error_code)
    {
        if (error_code == asio::error::operation_aborted)
            return;

        // Transient receive error: keep waiting, the retransmit timer drives further attempts.
        startReceive();
        return;
    }

    // Ignore datagrams from anything other than the gateway.
    if (sender_.address() != gateway_.address())
    {
        startReceive();
        return;
    }

    if (bytes < sizeof(ResponseHeader))
    {
        startReceive(); // Too short to be any response, ignore it.
        return;
    }

    const ResponseHeader& header = response_.header;

    // The response opcode echoes the opcode of the request we sent, with the high bit set.
    const quint8 request_opcode = std::visit([](const auto& request) { return request.opcode; }, request_);
    if (header.version != kNatPmpVersion || header.opcode != kResponseFlag + request_opcode)
    {
        startReceive(); // Not the response we are waiting for, ignore it.
        return;
    }

    // The datagram is a genuine response to our request, so stop retransmitting.
    retransmit_timer_.cancel();

    if (header.result_code != 0)
    {
        LOG(WARNING) << "NAT-PMP request rejected, result code:" << quint16(header.result_code);
        finishWithFailure(FROM_HERE);
        return;
    }

    if (phase_ == Phase::PUBLIC_ADDRESS)
        onPublicAddressResponse(bytes);
    else
        onMappingResponse(bytes);
}

//--------------------------------------------------------------------------------------------------
void NatPmpPortMapper::onPublicAddressResponse(size_t bytes)
{
    if (bytes < sizeof(PublicAddressResponse))
    {
        LOG(WARNING) << "NAT-PMP public address response is truncated";
        finishWithFailure(FROM_HERE);
        return;
    }

    const quint32 external_ip = quint32(response_.public_address.external_ip);
    external_address_ = QHostAddress(external_ip).toString();

    if (NetUtils::isPrivateIpAddress(external_address_))
    {
        LOG(INFO) << "NAT-PMP external address is private (double NAT/CGNAT), useless";
        finishWithFailure(FROM_HERE);
        return;
    }

    startPhase(Phase::MAPPING);
}

//--------------------------------------------------------------------------------------------------
void NatPmpPortMapper::onMappingResponse(size_t bytes)
{
    if (bytes < sizeof(MappingResponse))
    {
        LOG(WARNING) << "NAT-PMP mapping response is truncated";
        finishWithFailure(FROM_HERE);
        return;
    }

    external_port_ = response_.mapping.external_port;
    mapped_ = true;
    finished_ = true;

    LOG(INFO) << "NAT-PMP mapping added:" << external_address_ << ":" << external_port_;
    emit sig_ready(external_address_, external_port_);
}

//--------------------------------------------------------------------------------------------------
void NatPmpPortMapper::finishWithFailure(const Location& location)
{
    if (finished_)
        return;

    finished_ = true;

    std::error_code error_code;
    retransmit_timer_.cancel();
    socket_.cancel(error_code);

    LOG(INFO) << "NAT-PMP attempt is failed (" << location << ")";
    emit sig_failed();
}
