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

#include "net/adapter_enumerator.h"

#include "base/strings/string_printf.h"
#include "base/strings/unicode.h"

#include <ws2tcpip.h>
#include <iphlpapi.h>

namespace net {

namespace {

std::string addressToString(const SOCKET_ADDRESS& address)
{
    if (!address.lpSockaddr || address.iSockaddrLength <= 0)
        return std::string();

    char buffer[64] = { 0 };

    switch (address.lpSockaddr->sa_family)
    {
        case AF_INET:
        {
            sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(address.lpSockaddr);

            if (!inet_ntop(AF_INET, &addr->sin_addr, buffer, std::size(buffer)))
                return std::string();
        }
        break;

        case AF_INET6:
        {
            sockaddr_in6* addr = reinterpret_cast<sockaddr_in6*>(address.lpSockaddr);

            if (!inet_ntop(AF_INET6, &addr->sin6_addr, buffer, std::size(buffer)))
                return std::string();
        }
        break;

        default:
            return std::string();
    }

    return buffer;
}

} // namespace

//
// AdapterEnumerator
//

AdapterEnumerator::AdapterEnumerator()
{
    const ULONG flags = GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_SKIP_ANYCAST |
        GAA_FLAG_SKIP_MULTICAST;

    ULONG buffer_size = sizeof(IP_ADAPTER_ADDRESSES);

    adapters_buffer_ = std::make_unique<uint8_t[]>(buffer_size);
    adapter_ = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(adapters_buffer_.get());

    ULONG error_code = GetAdaptersAddresses(AF_INET, flags, nullptr, adapter_, &buffer_size);
    if (error_code != ERROR_SUCCESS)
    {
        if (error_code == ERROR_BUFFER_OVERFLOW)
        {
            adapters_buffer_ = std::make_unique<uint8_t[]>(buffer_size);
            adapter_ = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(adapters_buffer_.get());

            error_code = GetAdaptersAddresses(AF_INET, flags, nullptr, adapter_, &buffer_size);
            if (error_code != ERROR_SUCCESS)
            {
                adapters_buffer_.reset();
                adapter_ = nullptr;
                return;
            }
        }
        else
        {
            adapters_buffer_.reset();
            adapter_ = nullptr;
            return;
        }
    }
}

AdapterEnumerator::~AdapterEnumerator() = default;

bool AdapterEnumerator::isAtEnd() const
{
    return adapter_ == nullptr;
}

void AdapterEnumerator::advance()
{
    adapter_ = adapter_->Next;
}

std::string AdapterEnumerator::adapterName() const
{
    if (!adapter_->Description)
        return std::string();

    return base::utf8FromWide(adapter_->Description);
}

std::string AdapterEnumerator::connectionName() const
{
    if (!adapter_->FriendlyName)
        return std::string();

    return base::utf8FromWide(adapter_->FriendlyName);
}

std::string AdapterEnumerator::interfaceType() const
{
    switch (adapter_->IfType)
    {
        case IF_TYPE_OTHER:
            return "Other";

        case IF_TYPE_ETHERNET_CSMACD:
            return "Ethernet";

        case IF_TYPE_ISO88025_TOKENRING:
            return "Token Ring";

        case IF_TYPE_PPP:
            return "PPP";

        case IF_TYPE_SOFTWARE_LOOPBACK:
            return "Software Lookback";

        case IF_TYPE_ATM:
            return "ATM";

        case IF_TYPE_IEEE80211:
            return "IEEE 802.11 Wireless";

        case IF_TYPE_TUNNEL:
            return "Tunnel type encapsulation";

        case IF_TYPE_IEEE1394:
            return "IEEE 1394 Firewire";

        default:
            return std::string();
    }
}

uint32_t AdapterEnumerator::mtu() const
{
    return adapter_->Mtu;
}

uint64_t AdapterEnumerator::speed() const
{
    return adapter_->TransmitLinkSpeed;
}

std::string AdapterEnumerator::macAddress() const
{
    if (!adapter_->PhysicalAddressLength)
        return std::string();

    return base::stringPrintf("%.2X-%.2X-%.2X-%.2X-%.2X-%.2X",
                              adapter_->PhysicalAddress[0],
                              adapter_->PhysicalAddress[1],
                              adapter_->PhysicalAddress[2],
                              adapter_->PhysicalAddress[3],
                              adapter_->PhysicalAddress[4],
                              adapter_->PhysicalAddress[5],
                              adapter_->PhysicalAddress[6]);
}

bool AdapterEnumerator::isDhcp4Enabled() const
{
    return !!adapter_->Dhcpv4Enabled;
}

std::string AdapterEnumerator::dhcp4Server() const
{
    return addressToString(adapter_->Dhcpv4Server);
}

//
// IpAddressEnumerator
//

AdapterEnumerator::IpAddressEnumerator::IpAddressEnumerator(const AdapterEnumerator& adapter)
    : address_(adapter.adapter_->FirstUnicastAddress)
{
    // Nothing
}

bool AdapterEnumerator::IpAddressEnumerator::isAtEnd() const
{
    return address_ == nullptr;
}

void AdapterEnumerator::IpAddressEnumerator::advance()
{
    address_ = address_->Next;
}

std::string AdapterEnumerator::IpAddressEnumerator::address() const
{
    return addressToString(address_->Address);
}

std::string AdapterEnumerator::IpAddressEnumerator::mask() const
{
    in_addr addr;

    if (ConvertLengthToIpv4Mask(address_->OnLinkPrefixLength, &addr.s_addr) != NO_ERROR)
        return std::string();

    char buffer[64] = { 0 };

    if (!inet_ntop(AF_INET, &addr, buffer, std::size(buffer)))
        return std::string();

    return buffer;
}

//
// GatewayEnumerator
//

AdapterEnumerator::GatewayEnumerator::GatewayEnumerator(const AdapterEnumerator& adapter)
    : address_(adapter.adapter_->FirstGatewayAddress)
{
    // Nothing
}

bool AdapterEnumerator::GatewayEnumerator::isAtEnd() const
{
    return address_ == nullptr;
}

void AdapterEnumerator::GatewayEnumerator::advance()
{
    address_ = address_->Next;
}

std::string AdapterEnumerator::GatewayEnumerator::address() const
{
    return addressToString(address_->Address);
}

//
// DnsEnumerator
//

AdapterEnumerator::DnsEnumerator::DnsEnumerator(const AdapterEnumerator& adapter)
    : address_(adapter.adapter_->FirstDnsServerAddress)
{
    // Nothing
}

bool AdapterEnumerator::DnsEnumerator::isAtEnd() const
{
    return address_ == nullptr;
}

void AdapterEnumerator::DnsEnumerator::advance()
{
    address_ = address_->Next;
}

std::string AdapterEnumerator::DnsEnumerator::address() const
{
    return addressToString(address_->Address);
}

} // namespace net
