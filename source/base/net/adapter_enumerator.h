//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__NET__ADAPTER_ENUMERATOR_H
#define BASE__NET__ADAPTER_ENUMERATOR_H

#include "base/macros_magic.h"

#include <memory>
#include <string>

struct _IP_ADAPTER_ADDRESSES_LH;
struct _IP_ADAPTER_UNICAST_ADDRESS_LH;
struct _IP_ADAPTER_DNS_SERVER_ADDRESS_XP;
struct _IP_ADAPTER_GATEWAY_ADDRESS_LH;

namespace base {

class AdapterEnumerator
{
public:
    AdapterEnumerator();
    ~AdapterEnumerator();

    bool isAtEnd() const;
    void advance();

    std::string adapterName() const;
    std::string connectionName() const;
    std::string interfaceType() const;
    uint32_t mtu() const;
    uint64_t speed() const;
    std::string macAddress() const;

    bool isDhcp4Enabled() const;
    std::string dhcp4Server() const;

    class IpAddressEnumerator
    {
    public:
        explicit IpAddressEnumerator(const AdapterEnumerator& adapter);

        bool isAtEnd() const;
        void advance();

        std::string address() const;
        std::string mask() const;

    private:
        const _IP_ADAPTER_UNICAST_ADDRESS_LH* address_;

        DISALLOW_COPY_AND_ASSIGN(IpAddressEnumerator);
    };

    class GatewayEnumerator
    {
    public:
        explicit GatewayEnumerator(const AdapterEnumerator& adapter);

        bool isAtEnd() const;
        void advance();
        std::string address() const;

    private:
        const _IP_ADAPTER_GATEWAY_ADDRESS_LH* address_;

        DISALLOW_COPY_AND_ASSIGN(GatewayEnumerator);
    };

    class DnsEnumerator
    {
    public:
        explicit DnsEnumerator(const AdapterEnumerator& adapter);

        bool isAtEnd() const;
        void advance();
        std::string address() const;

    private:
        const _IP_ADAPTER_DNS_SERVER_ADDRESS_XP* address_ = nullptr;

        DISALLOW_COPY_AND_ASSIGN(DnsEnumerator);
    };

private:
    std::unique_ptr<uint8_t[]> adapters_buffer_;
    _IP_ADAPTER_ADDRESSES_LH* adapter_;

    DISALLOW_COPY_AND_ASSIGN(AdapterEnumerator);
};

} // namespace base

#endif // BASE__NET__ADAPTER_ENUMERATOR_H
