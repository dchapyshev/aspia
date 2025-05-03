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

#ifndef BASE_NET_ADAPTER_ENUMERATOR_H
#define BASE_NET_ADAPTER_ENUMERATOR_H

#include "base/macros_magic.h"

#include <QByteArray>
#include <QString>

#if defined(Q_OS_WINDOWS)
struct _IP_ADAPTER_ADDRESSES_LH;
struct _IP_ADAPTER_UNICAST_ADDRESS_LH;
struct _IP_ADAPTER_DNS_SERVER_ADDRESS_XP;
struct _IP_ADAPTER_GATEWAY_ADDRESS_LH;
#endif // defined(Q_OS_WINDOWS)

namespace base {

class AdapterEnumerator
{
public:
    AdapterEnumerator();
    ~AdapterEnumerator();

    bool isAtEnd() const;
    void advance();

    QString adapterName() const;
    QString connectionName() const;
    QString interfaceType() const;
    quint32 mtu() const;
    quint64 speed() const;
    QString macAddress() const;

    bool isDhcp4Enabled() const;
    QString dhcp4Server() const;

    class IpAddressEnumerator
    {
    public:
        explicit IpAddressEnumerator(const AdapterEnumerator& adapter);

        bool isAtEnd() const;
        void advance();

        QString address() const;
        QString mask() const;

    private:
#if defined(Q_OS_WINDOWS)
        const _IP_ADAPTER_UNICAST_ADDRESS_LH* address_;
#endif

        DISALLOW_COPY_AND_ASSIGN(IpAddressEnumerator);
    };

    class GatewayEnumerator
    {
    public:
        explicit GatewayEnumerator(const AdapterEnumerator& adapter);

        bool isAtEnd() const;
        void advance();
        QString address() const;

    private:
#if defined(Q_OS_WINDOWS)
        const _IP_ADAPTER_GATEWAY_ADDRESS_LH* address_;
#endif

        DISALLOW_COPY_AND_ASSIGN(GatewayEnumerator);
    };

    class DnsEnumerator
    {
    public:
        explicit DnsEnumerator(const AdapterEnumerator& adapter);

        bool isAtEnd() const;
        void advance();
        QString address() const;

    private:
#if defined(Q_OS_WINDOWS)
        const _IP_ADAPTER_DNS_SERVER_ADDRESS_XP* address_ = nullptr;
#endif

        DISALLOW_COPY_AND_ASSIGN(DnsEnumerator);
    };

private:
#if defined(Q_OS_WINDOWS)
    QByteArray adapters_buffer_;
    _IP_ADAPTER_ADDRESSES_LH* adapter_;
#endif

    DISALLOW_COPY_AND_ASSIGN(AdapterEnumerator);
};

} // namespace base

#endif // BASE_NET_ADAPTER_ENUMERATOR_H
