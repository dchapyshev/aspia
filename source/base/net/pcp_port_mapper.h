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

#ifndef BASE_NET_PCP_PORT_MAPPER_H
#define BASE_NET_PCP_PORT_MAPPER_H

#include <QObject>
#include <QtEndian>

#include <asio/ip/udp.hpp>
#include <asio/steady_timer.hpp>

#include <array>
#include <variant>

#include "base/shared_pointer.h"

class Location;

// Asynchronous client for the gateway's native UDP port-mapping protocols on port 5351. PCP
// (RFC 6887) is tried first; if the gateway does not support it, the mapper falls back to NAT-PMP
// (RFC 6886) on the same socket. On success sig_ready is emitted with the external address and the
// mapped port; otherwise sig_failed is emitted. The mapping is dropped when the object is destroyed.
class PcpPortMapper final : public QObject
{
    Q_OBJECT

public:
    explicit PcpPortMapper(QObject* parent = nullptr);
    ~PcpPortMapper() final;

    void addUdpMapping(quint16 internal_port);

signals:
    void sig_ready(const QString& external_address, quint16 external_port);
    void sig_failed();

private:
    enum class Type { NONE, PCP, NATPMP };
    Q_ENUM(Type)

    enum class Phase { PCP, NATPMP_PUBLIC_ADDRESS, NATPMP_MAPPING };
    Q_ENUM(Phase)

    // NAT-PMP (RFC 6886) wire messages, version 0. Multi-byte fields are big-endian; quint*_be
    // convert on access. Fields are naturally aligned, so the in-memory layout matches the wire
    // layout (asserted below).
    struct NatPmpPublicAddressRequest
    {
        quint8 version;
        quint8 opcode;
    };

    struct NatPmpMappingRequest
    {
        quint8 version;
        quint8 opcode;
        quint16_be reserved;
        quint16_be internal_port;
        quint16_be external_port;
        quint32_be lifetime;
    };

    struct NatPmpResponseHeader
    {
        quint8 version;
        quint8 opcode;
        quint16_be result_code;
        quint32_be epoch;
    };

    struct NatPmpPublicAddressResponse
    {
        NatPmpResponseHeader header;
        quint32_be external_ip;
    };

    struct NatPmpMappingResponse
    {
        NatPmpResponseHeader header;
        quint16_be internal_port;
        quint16_be external_port;
        quint32_be lifetime;
    };

    // PCP (RFC 6887) wire messages, version 2. Addresses are 16-byte IPv6 (IPv4 is sent IPv4-mapped).
    struct PcpRequestHeader
    {
        quint8 version;
        quint8 opcode; // High bit (R) is 0 for a request.
        quint16_be reserved;
        quint32_be lifetime;
        quint8 client_ip[16];
    };

    struct PcpResponseHeader
    {
        quint8 version;
        quint8 opcode; // High bit (R) is 1 for a response.
        quint8 reserved;
        quint8 result_code;
        quint32_be lifetime;
        quint32_be epoch;
        quint8 reserved2[12];
    };

    struct PcpMapData
    {
        quint8 nonce[12];
        quint8 protocol;
        quint8 reserved[3];
        quint16_be internal_port;
        quint16_be external_port;
        quint8 external_ip[16];
    };

    struct PcpMapRequest
    {
        PcpRequestHeader header;
        PcpMapData data;
    };

    struct PcpMapResponse
    {
        PcpResponseHeader header;
        PcpMapData data;
    };

    static_assert(sizeof(NatPmpPublicAddressRequest) == 2);
    static_assert(sizeof(NatPmpMappingRequest) == 12);
    static_assert(sizeof(NatPmpPublicAddressResponse) == 12);
    static_assert(sizeof(NatPmpMappingResponse) == 16);
    static_assert(sizeof(PcpRequestHeader) == 24);
    static_assert(sizeof(PcpResponseHeader) == 24);
    static_assert(sizeof(PcpMapData) == 36);
    static_assert(sizeof(PcpMapRequest) == 60);
    static_assert(sizeof(PcpMapResponse) == 60);

    using Request = std::variant<PcpMapRequest, NatPmpPublicAddressRequest, NatPmpMappingRequest>;

    union Response
    {
        NatPmpResponseHeader natpmp_header;
        NatPmpPublicAddressResponse public_address;
        NatPmpMappingResponse mapping;
        PcpResponseHeader pcp_header;
        PcpMapResponse pcp_map;
    };

    //----------------------------------------------------------------------------------------------
    // Generic
    //----------------------------------------------------------------------------------------------
    void startPhase(Phase phase);
    void sendRequest();
    void startRetransmitTimer();
    void startReceive();
    void onResponse(const std::error_code& error_code, size_t bytes);
    void finishWithFailure(const Location& location);

    //----------------------------------------------------------------------------------------------
    // PCP
    //----------------------------------------------------------------------------------------------
    PcpMapRequest makePcpMapRequest(quint32 lifetime) const;
    void onPcpResponse(size_t bytes);

    //----------------------------------------------------------------------------------------------
    // NAT-PMP
    //----------------------------------------------------------------------------------------------
    void fallbackToNatPmp();
    void onNatPmpResponse(size_t bytes);
    void onPublicAddressResponse(size_t bytes);
    void onMappingResponse(size_t bytes);

    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint gateway_;
    asio::steady_timer retransmit_timer_;

    Request request_ {};
    Response response_ {};

    Type type_ = Type::NONE;
    Phase phase_ = Phase::PCP;
    int tries_ = 0;
    bool finished_ = false;

    quint16 internal_port_ = 0;
    quint16 external_port_ = 0;
    QString external_address_;

    std::array<quint8, 16> client_ip_ {}; // Local address as IPv4-mapped IPv6 (for PCP).
    std::array<quint8, 12> nonce_ {};     // PCP mapping nonce.

    SharedPointer<bool> alive_guard_ { new bool(true) };

    Q_DISABLE_COPY_MOVE(PcpPortMapper)
};

#endif // BASE_NET_PCP_PORT_MAPPER_H
