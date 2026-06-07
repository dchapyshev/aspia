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

#ifndef BASE_NET_NAT_PMP_PORT_MAPPER_H
#define BASE_NET_NAT_PMP_PORT_MAPPER_H

#include <QObject>
#include <QtEndian>

#include <asio/ip/udp.hpp>
#include <asio/steady_timer.hpp>

#include <variant>

#include "base/shared_pointer.h"

class Location;

// Asynchronous NAT-PMP (RFC 6886) client. Asks the default gateway to forward its external
// |internal_port| to this host's |internal_port| over UDP. Runs entirely on the shared ASIO
// io_context (no worker thread). On success sig_ready is emitted with the external address and the
// mapped port; otherwise sig_failed is emitted. The mapping is dropped when the object is destroyed.
class NatPmpPortMapper final : public QObject
{
    Q_OBJECT

public:
    explicit NatPmpPortMapper(QObject* parent = nullptr);
    ~NatPmpPortMapper() final;

    void addUdpMapping(quint16 internal_port);

signals:
    void sig_ready(const QString& external_address, quint16 external_port);
    void sig_failed();

private:
    enum class Phase { PUBLIC_ADDRESS, MAPPING };

    // NAT-PMP wire messages. Multi-byte fields are big-endian; quint*_be convert on access. The
    // fields are naturally aligned, so the in-memory layout matches the wire layout (asserted below).
    struct PublicAddressRequest
    {
        quint8 version;
        quint8 opcode;
    };

    struct MappingRequest
    {
        quint8 version;
        quint8 opcode;
        quint16_be reserved;
        quint16_be internal_port;
        quint16_be external_port;
        quint32_be lifetime;
    };

    struct ResponseHeader
    {
        quint8 version;
        quint8 opcode;
        quint16_be result_code;
        quint32_be epoch;
    };

    struct PublicAddressResponse
    {
        ResponseHeader header;
        quint32_be external_ip;
    };

    struct MappingResponse
    {
        ResponseHeader header;
        quint16_be internal_port;
        quint16_be external_port;
        quint32_be lifetime;
    };

    static_assert(sizeof(PublicAddressRequest) == 2);
    static_assert(sizeof(MappingRequest) == 12);
    static_assert(sizeof(PublicAddressResponse) == 12);
    static_assert(sizeof(MappingResponse) == 16);

    using Request = std::variant<PublicAddressRequest, MappingRequest>;

    union Response
    {
        ResponseHeader header;
        PublicAddressResponse public_address;
        MappingResponse mapping;
    };

    void startPhase(Phase phase);
    void sendRequest();
    void startRetransmitTimer();
    void startReceive();
    void onResponse(const std::error_code& error_code, size_t bytes);
    void onPublicAddressResponse(size_t bytes);
    void onMappingResponse(size_t bytes);
    void finishWithFailure(const Location& location);

    asio::io_context& io_context_;
    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint gateway_;
    asio::ip::udp::endpoint sender_;
    asio::steady_timer retransmit_timer_;

    Request request_ {};
    Response response_ {};

    Phase phase_ = Phase::PUBLIC_ADDRESS;
    int tries_ = 0;
    bool finished_ = false;
    bool mapped_ = false;

    quint16 internal_port_ = 0;
    quint16 external_port_ = 0;
    QString external_address_;

    SharedPointer<bool> alive_guard_ { new bool(true) };

    Q_DISABLE_COPY_MOVE(NatPmpPortMapper)
};

#endif // BASE_NET_NAT_PMP_PORT_MAPPER_H
