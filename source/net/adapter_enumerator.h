//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef NET__ADAPTER_ENUMERATOR_H
#define NET__ADAPTER_ENUMERATOR_H

#include "base/macros_magic.h"

#include <memory>
#include <string>

struct _IP_ADAPTER_INFO;
struct _IP_ADDR_STRING;

namespace net {

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
    uint32_t speed() const;
    std::string macAddress() const;
    bool isWinsEnabled() const;
    std::string primaryWinsServer() const;
    std::string secondaryWinsServer() const;
    bool isDhcpEnabled() const;

    class IpAddressEnumerator
    {
    public:
        explicit IpAddressEnumerator(const AdapterEnumerator& adapter);

        bool isAtEnd() const;
        void advance();

        std::string address() const;
        std::string mask() const;

    private:
        _IP_ADDR_STRING* address_;

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
        _IP_ADDR_STRING* address_;

        DISALLOW_COPY_AND_ASSIGN(GatewayEnumerator);
    };

    class DhcpEnumerator
    {
    public:
        explicit DhcpEnumerator(const AdapterEnumerator& adapter);

        bool isAtEnd() const;
        void advance();
        std::string address() const;

    private:
        _IP_ADDR_STRING* address_;

        DISALLOW_COPY_AND_ASSIGN(DhcpEnumerator);
    };

    class DnsEnumerator
    {
    public:
        explicit DnsEnumerator(const AdapterEnumerator& adapter);

        bool isAtEnd() const;
        void advance();
        std::string address() const;

    private:
        std::unique_ptr<uint8_t[]> info_buffer_;
        _IP_ADDR_STRING* address_ = nullptr;

        DISALLOW_COPY_AND_ASSIGN(DnsEnumerator);
    };

private:
    std::unique_ptr<uint8_t[]> adapters_buffer_;
    _IP_ADAPTER_INFO* adapter_;

    DISALLOW_COPY_AND_ASSIGN(AdapterEnumerator);
};

} // namespace net

#endif // NET__ADAPTER_ENUMERATOR_H
