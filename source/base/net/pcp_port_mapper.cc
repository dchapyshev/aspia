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

#include "base/net/pcp_port_mapper.h"

#include <QHostAddress>

#include <chrono>
#include <cstring>

#include "base/location.h"
#include "base/logging.h"
#include "base/crypto/random.h"
#include "base/net/net_utils.h"
#include "base/net/udp_channel.h"
#include "base/threading/asio_event_dispatcher.h"

namespace {

const quint16 kGatewayPort = 5351;
const quint32 kLeaseSeconds = 3600;

const int kInitialTimeoutMs = 250;
const int kMaxTries = 4;

const quint8 kNatPmpVersion = 0;
const quint8 kNatPmpResponseFlag = 128; // NAT-PMP responses echo the request opcode with this set.

enum NatPmpOpCode : quint8
{
    NATPMP_PUBLIC_ADDRESS = 0,
    NATPMP_MAP_UDP = 1
};

const quint8 kPcpVersion = 2;
const quint8 kPcpOpcodeMap = 1;      // R bit is 0 for a request.
const quint8 kPcpResponseBit = 0x80; // R bit set in a response opcode byte.
const quint8 kPcpProtocolUdp = 17;
const quint8 kPcpResultSuccess = 0;

//--------------------------------------------------------------------------------------------------
// Extracts the IPv4 from an IPv4-mapped IPv6 address (::ffff:a.b.c.d). Empty if it is not mapped.
QString ipv4FromMapped(const quint8 address[16])
{
    bool ok = false;
    const quint32 ipv4 = QHostAddress(address).toIPv4Address(&ok);
    return ok ? QHostAddress(ipv4).toString() : QString();
}

} // namespace

//--------------------------------------------------------------------------------------------------
PcpPortMapper::PcpPortMapper(QObject* parent)
    : QObject(parent),
      socket_(AsioEventDispatcher::ioContext()),
      retransmit_timer_(AsioEventDispatcher::ioContext())
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
PcpPortMapper::~PcpPortMapper()
{
    io_->alive = false;

    // Best-effort removal with lifetime 0, sent synchronously over the connected socket; the
    // response is not awaited. The protocol that established the mapping is the one used to drop it.
    if (type_ == Type::PCP)
    {
        const PcpMapRequest request = makePcpMapRequest(0);

        std::error_code error_code;
        socket_.send(asio::buffer(&request, sizeof(request)), 0, error_code);
    }
    else if (type_ == Type::NATPMP)
    {
        NatPmpMappingRequest request = {};
        request.version = kNatPmpVersion;
        request.opcode = NATPMP_MAP_UDP;
        request.internal_port = internal_port_;

        std::error_code error_code;
        socket_.send(asio::buffer(&request, sizeof(request)), 0, error_code);
    }
}

//--------------------------------------------------------------------------------------------------
void PcpPortMapper::addUdpMapping(quint16 internal_port, quint32 methods)
{
    internal_port_ = internal_port;

    const bool pcp = (methods & UDP_METHOD_PCP) != 0;
    const bool natpmp = (methods & UDP_METHOD_NAT_PMP) != 0;

    if (pcp && natpmp)
        protocols_ = Protocols::PCP_THEN_NATPMP;
    else if (pcp)
        protocols_ = Protocols::PCP_ONLY;
    else
        protocols_ = Protocols::NATPMP_ONLY;

    const QString gateway = NetUtils::defaultGatewayAddress();
    if (gateway.isEmpty())
    {
        LOG(INFO) << "No default gateway found";
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

    gateway_ = asio::ip::udp::endpoint(address, kGatewayPort);

    socket_.open(asio::ip::udp::v4(), error_code);
    if (error_code)
    {
        LOG(WARNING) << "Unable to open socket:" << error_code;
        finishWithFailure(FROM_HERE);
        return;
    }

    // Connecting the UDP socket fixes the destination (so only gateway datagrams are received, and
    // send/receive need no endpoint) and reveals the local source address that PCP must advertise.
    socket_.connect(gateway_, error_code);
    if (error_code)
    {
        LOG(WARNING) << "Unable to connect socket:" << error_code;
        finishWithFailure(FROM_HERE);
        return;
    }

    const asio::ip::udp::endpoint local = socket_.local_endpoint(error_code);
    if (error_code || !local.address().is_v4())
    {
        LOG(WARNING) << "Unable to determine local address";
        finishWithFailure(FROM_HERE);
        return;
    }

    // The PCP client address is the local IPv4 in IPv4-mapped IPv6 form (::ffff:a.b.c.d).
    client_ip_ = asio::ip::make_address_v6(asio::ip::v4_mapped, local.address().to_v4()).to_bytes();

    Random::fillBuffer(nonce_.data(), nonce_.size());

    // PCP is tried first unless only NAT-PMP is allowed. The orchestrator only creates this mapper
    // when at least one of the two protocols is allowed.
    startPhase(protocols_ == Protocols::NATPMP_ONLY ? Phase::NATPMP_PUBLIC_ADDRESS : Phase::PCP);
}

//--------------------------------------------------------------------------------------------------
void PcpPortMapper::startPhase(Phase phase)
{
    phase_ = phase;
    tries_ = 0;

    switch (phase)
    {
        case Phase::PCP:
        {
            io_->request = makePcpMapRequest(kLeaseSeconds);
        }
        break;

        case Phase::NATPMP_PUBLIC_ADDRESS:
        {
            NatPmpPublicAddressRequest request = {};
            request.version = kNatPmpVersion;
            request.opcode = NATPMP_PUBLIC_ADDRESS;
            io_->request = request;
        }
        break;

        case Phase::NATPMP_MAPPING:
        {
            NatPmpMappingRequest request = {};
            request.version = kNatPmpVersion;
            request.opcode = NATPMP_MAP_UDP;
            request.internal_port = internal_port_;
            request.external_port = internal_port_;
            request.lifetime = kLeaseSeconds;
            io_->request = request;
        }
        break;
    }

    startReceive();
    sendRequest();
}

//--------------------------------------------------------------------------------------------------
void PcpPortMapper::sendRequest()
{
    auto io = io_;

    // The variant carries the active request type, so its wire size is known statically here.
    std::visit([this, io](const auto& request)
    {
        socket_.async_send(asio::buffer(&request, sizeof(request)),
            [this, io](const std::error_code& error_code, size_t /* bytes */)
        {
            if (!io->alive || finished_)
                return;

            if (error_code)
            {
                finishWithFailure(FROM_HERE);
                return;
            }

            startRetransmitTimer();
        });
    }, io_->request);
}

//--------------------------------------------------------------------------------------------------
void PcpPortMapper::startRetransmitTimer()
{
    retransmit_timer_.expires_after(std::chrono::milliseconds(kInitialTimeoutMs << tries_));

    auto io = io_;
    retransmit_timer_.async_wait([this, io](const std::error_code& error_code)
    {
        // A cancelled timer (response arrived) reports operation_aborted.
        if (!io->alive || finished_ || error_code)
            return;

        if (++tries_ < kMaxTries)
        {
            sendRequest();
            return;
        }

        // No reply. PCP is optional, so fall back to NAT-PMP; a NAT-PMP timeout is terminal.
        if (phase_ == Phase::PCP)
        {
            LOG(INFO) << "No PCP response from gateway, falling back to NAT-PMP";
            fallbackToNatPmp();
        }
        else
        {
            finishWithFailure(FROM_HERE);
        }
    });
}

//--------------------------------------------------------------------------------------------------
void PcpPortMapper::startReceive()
{
    auto io = io_;
    socket_.async_receive(asio::buffer(&io_->response, sizeof(io_->response)),
        [this, io](const std::error_code& error_code, size_t bytes)
    {
        if (!io->alive)
            return;

        onResponse(error_code, bytes);
    });
}

//--------------------------------------------------------------------------------------------------
void PcpPortMapper::onResponse(const std::error_code& error_code, size_t bytes)
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

    if (phase_ == Phase::PCP)
        onPcpResponse(bytes);
    else
        onNatPmpResponse(bytes);
}

//--------------------------------------------------------------------------------------------------
void PcpPortMapper::finishWithFailure(const Location& location)
{
    if (finished_)
        return;

    finished_ = true;

    std::error_code error_code;
    retransmit_timer_.cancel();
    socket_.cancel(error_code);

    LOG(INFO) << "Gateway port mapping failed (" << location << ")";
    emit sig_failed();
}

//--------------------------------------------------------------------------------------------------
PcpPortMapper::PcpMapRequest PcpPortMapper::makePcpMapRequest(quint32 lifetime) const
{
    PcpMapRequest request = {};
    request.header.version = kPcpVersion;
    request.header.opcode = kPcpOpcodeMap;
    request.header.lifetime = lifetime;
    memcpy(request.header.client_ip, client_ip_.data(), client_ip_.size());

    memcpy(request.data.nonce, nonce_.data(), nonce_.size());
    request.data.protocol = kPcpProtocolUdp;
    request.data.internal_port = internal_port_;
    request.data.external_port = internal_port_; // Suggested; the gateway may assign another.

    return request;
}

//--------------------------------------------------------------------------------------------------
void PcpPortMapper::onPcpResponse(size_t bytes)
{
    if (bytes < sizeof(PcpResponseHeader))
    {
        startReceive(); // Too short to be a PCP response, ignore it.
        return;
    }

    const PcpResponseHeader& header = io_->response.pcp_header;

    // A NAT-PMP-only gateway answers our PCP probe with a different version; fall back to NAT-PMP.
    if (header.version != kPcpVersion || header.opcode != (kPcpResponseBit | kPcpOpcodeMap))
    {
        fallbackToNatPmp();
        return;
    }

    if (header.result_code != kPcpResultSuccess)
    {
        LOG(INFO) << "Gateway rejected PCP (result code" << quint16(header.result_code)
                  << "), falling back to NAT-PMP";
        fallbackToNatPmp();
        return;
    }

    // A success response must match the request we sent by protocol, internal port and nonce
    // (RFC 6887 11.4); the internal IP is matched implicitly by the connected socket. A truncated or
    // mismatched datagram is not our mapping, so keep waiting (the retransmit timer drives further
    // attempts). The size check short-circuits to avoid reading fields out of a truncated datagram.
    const PcpMapData& map = io_->response.pcp_map.data;
    if (bytes < sizeof(PcpMapResponse) ||
        map.protocol != kPcpProtocolUdp ||
        map.internal_port != internal_port_ ||
        memcmp(map.nonce, nonce_.data(), nonce_.size()) != 0)
    {
        startReceive();
        return;
    }

    retransmit_timer_.cancel();

    external_port_ = map.external_port;
    external_address_ = ipv4FromMapped(map.external_ip);

    if (NetUtils::isPrivateIpAddress(external_address_))
    {
        LOG(INFO) << "PCP external address is private (double NAT/CGNAT), useless";
        finishWithFailure(FROM_HERE);
        return;
    }

    type_ = Type::PCP;
    finished_ = true;

    LOG(INFO) << "PCP mapping added:" << external_address_ << ":" << external_port_;
    emit sig_ready(external_address_, external_port_);
}

//--------------------------------------------------------------------------------------------------
void PcpPortMapper::fallbackToNatPmp()
{
    if (protocols_ == Protocols::PCP_ONLY)
    {
        finishWithFailure(FROM_HERE);
        return;
    }

    std::error_code error_code;
    retransmit_timer_.cancel();
    socket_.cancel(error_code);

    startPhase(Phase::NATPMP_PUBLIC_ADDRESS);
}

//--------------------------------------------------------------------------------------------------
void PcpPortMapper::onNatPmpResponse(size_t bytes)
{
    if (bytes < sizeof(NatPmpResponseHeader))
    {
        startReceive(); // Too short to be any response, ignore it.
        return;
    }

    const NatPmpResponseHeader& header = io_->response.natpmp_header;

    const quint8 sent_opcode =
        (phase_ == Phase::NATPMP_PUBLIC_ADDRESS) ? NATPMP_PUBLIC_ADDRESS : NATPMP_MAP_UDP;
    if (header.version != kNatPmpVersion || header.opcode != kNatPmpResponseFlag + sent_opcode)
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

    if (phase_ == Phase::NATPMP_PUBLIC_ADDRESS)
        onPublicAddressResponse(bytes);
    else
        onMappingResponse(bytes);
}

//--------------------------------------------------------------------------------------------------
void PcpPortMapper::onPublicAddressResponse(size_t bytes)
{
    if (bytes < sizeof(NatPmpPublicAddressResponse))
    {
        LOG(WARNING) << "NAT-PMP public address response is truncated";
        finishWithFailure(FROM_HERE);
        return;
    }

    external_address_ = QHostAddress(quint32(io_->response.public_address.external_ip)).toString();

    if (NetUtils::isPrivateIpAddress(external_address_))
    {
        LOG(INFO) << "NAT-PMP external address is private (double NAT/CGNAT), useless";
        finishWithFailure(FROM_HERE);
        return;
    }

    startPhase(Phase::NATPMP_MAPPING);
}

//--------------------------------------------------------------------------------------------------
void PcpPortMapper::onMappingResponse(size_t bytes)
{
    if (bytes < sizeof(NatPmpMappingResponse))
    {
        LOG(WARNING) << "NAT-PMP mapping response is truncated";
        finishWithFailure(FROM_HERE);
        return;
    }

    external_port_ = io_->response.mapping.external_port;
    type_ = Type::NATPMP;
    finished_ = true;

    LOG(INFO) << "NAT-PMP mapping added:" << external_address_ << ":" << external_port_;
    emit sig_ready(external_address_, external_port_);
}
