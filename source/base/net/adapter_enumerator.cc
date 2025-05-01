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

#include "base/net/adapter_enumerator.h"

#include "base/logging.h"
#include "base/strings/unicode.h"

#include <fmt/format.h>

#if defined(OS_WIN)
#include <WS2tcpip.h>
#include <iphlpapi.h>
#endif // defined(OS_WIN)

namespace base {

namespace {

#if defined(OS_WIN)
//--------------------------------------------------------------------------------------------------
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
#endif // defined(OS_WIN)

} // namespace

//
// AdapterEnumerator
//

//--------------------------------------------------------------------------------------------------
AdapterEnumerator::AdapterEnumerator()
{
#if defined(OS_WIN)
    const ULONG flags = GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_SKIP_ANYCAST |
        GAA_FLAG_SKIP_MULTICAST;

    ULONG buffer_size = sizeof(IP_ADAPTER_ADDRESSES);

    adapters_buffer_.resize(buffer_size);
    adapter_ = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(adapters_buffer_.data());

    ULONG error_code = GetAdaptersAddresses(AF_INET, flags, nullptr, adapter_, &buffer_size);
    if (error_code != ERROR_SUCCESS)
    {
        if (error_code == ERROR_BUFFER_OVERFLOW)
        {
            adapters_buffer_.resize(buffer_size);
            adapter_ = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(adapters_buffer_.data());

            error_code = GetAdaptersAddresses(AF_INET, flags, nullptr, adapter_, &buffer_size);
            if (error_code != ERROR_SUCCESS)
            {
                adapters_buffer_.clear();
                adapter_ = nullptr;
                return;
            }
        }
        else
        {
            adapters_buffer_.clear();
            adapter_ = nullptr;
            return;
        }
    }
#else
    NOTIMPLEMENTED();
#endif
}

//--------------------------------------------------------------------------------------------------
AdapterEnumerator::~AdapterEnumerator() = default;

//--------------------------------------------------------------------------------------------------
bool AdapterEnumerator::isAtEnd() const
{
#if defined(OS_WIN)
    return adapter_ == nullptr;
#else
    NOTIMPLEMENTED();
    return true;
#endif
}

//--------------------------------------------------------------------------------------------------
void AdapterEnumerator::advance()
{
#if defined(OS_WIN)
    adapter_ = adapter_->Next;
#else
    NOTIMPLEMENTED();
#endif
}

//--------------------------------------------------------------------------------------------------
std::string AdapterEnumerator::adapterName() const
{
#if defined(OS_WIN)
    if (!adapter_->Description)
        return std::string();

    return base::utf8FromWide(adapter_->Description);
#else
    NOTIMPLEMENTED();
    return std::string();
#endif
}

//--------------------------------------------------------------------------------------------------
std::string AdapterEnumerator::connectionName() const
{
#if defined(OS_WIN)
    if (!adapter_->FriendlyName)
        return std::string();

    return base::utf8FromWide(adapter_->FriendlyName);
#else
    NOTIMPLEMENTED();
    return std::string();
#endif
}

//--------------------------------------------------------------------------------------------------
std::string AdapterEnumerator::interfaceType() const
{
#if defined(OS_WIN)
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
#else
    NOTIMPLEMENTED();
    return std::string();
#endif
}

//--------------------------------------------------------------------------------------------------
uint32_t AdapterEnumerator::mtu() const
{
#if defined(OS_WIN)
    return adapter_->Mtu;
#else
    NOTIMPLEMENTED();
    return 0;
#endif
}

//--------------------------------------------------------------------------------------------------
quint64 AdapterEnumerator::speed() const
{
#if defined(OS_WIN)
    if (adapter_->TransmitLinkSpeed == std::numeric_limits<ULONG64>::max())
        return 0;

    return adapter_->TransmitLinkSpeed;
#else
    NOTIMPLEMENTED();
    return 0;
#endif
}

//--------------------------------------------------------------------------------------------------
std::string AdapterEnumerator::macAddress() const
{
#if defined(OS_WIN)
    if (!adapter_->PhysicalAddressLength)
        return std::string();

    return fmt::format("{:02x}-{:02x}-{:02x}-{:02x}-{:02x}-{:02x}",
                       adapter_->PhysicalAddress[0],
                       adapter_->PhysicalAddress[1],
                       adapter_->PhysicalAddress[2],
                       adapter_->PhysicalAddress[3],
                       adapter_->PhysicalAddress[4],
                       adapter_->PhysicalAddress[5]);
#else
    NOTIMPLEMENTED();
    return std::string();
#endif
}

//--------------------------------------------------------------------------------------------------
bool AdapterEnumerator::isDhcp4Enabled() const
{
#if defined(OS_WIN)
    return !!adapter_->Dhcpv4Enabled;
#else
    NOTIMPLEMENTED();
    return false;
#endif
}

//--------------------------------------------------------------------------------------------------
std::string AdapterEnumerator::dhcp4Server() const
{
#if defined(OS_WIN)
    return addressToString(adapter_->Dhcpv4Server);
#else
    NOTIMPLEMENTED();
    return std::string();
#endif
}

//
// IpAddressEnumerator
//

AdapterEnumerator::IpAddressEnumerator::IpAddressEnumerator(const AdapterEnumerator& adapter)
#if defined(OS_WIN)
    : address_(adapter.adapter_->FirstUnicastAddress)
#endif // defined(OS_WIN)
{
    // Nothing
}

bool AdapterEnumerator::IpAddressEnumerator::isAtEnd() const
{
#if defined(OS_WIN)
    return address_ == nullptr;
#else
    NOTIMPLEMENTED();
    return true;
#endif
}

void AdapterEnumerator::IpAddressEnumerator::advance()
{
#if defined(OS_WIN)
    address_ = address_->Next;
#else
    NOTIMPLEMENTED();
#endif
}

std::string AdapterEnumerator::IpAddressEnumerator::address() const
{
#if defined(OS_WIN)
    return addressToString(address_->Address);
#else
    NOTIMPLEMENTED();
    return std::string();
#endif
}

std::string AdapterEnumerator::IpAddressEnumerator::mask() const
{
#if defined(OS_WIN)
    in_addr addr;

    if (ConvertLengthToIpv4Mask(address_->OnLinkPrefixLength, &addr.s_addr) != NO_ERROR)
        return std::string();

    char buffer[64] = { 0 };

    if (!inet_ntop(AF_INET, &addr, buffer, std::size(buffer)))
        return std::string();

    return buffer;
#else
    NOTIMPLEMENTED();
    return std::string();
#endif
}

//
// GatewayEnumerator
//

AdapterEnumerator::GatewayEnumerator::GatewayEnumerator(const AdapterEnumerator& adapter)
#if defined(OS_WIN)
    : address_(adapter.adapter_->FirstGatewayAddress)
#endif
{
    // Nothing
}

bool AdapterEnumerator::GatewayEnumerator::isAtEnd() const
{
#if defined(OS_WIN)
    return address_ == nullptr;
#else
    NOTIMPLEMENTED();
    return true;
#endif
}

void AdapterEnumerator::GatewayEnumerator::advance()
{
#if defined(OS_WIN)
    address_ = address_->Next;
#else
    NOTIMPLEMENTED();
#endif
}

std::string AdapterEnumerator::GatewayEnumerator::address() const
{
#if defined(OS_WIN)
    return addressToString(address_->Address);
#else
    NOTIMPLEMENTED();
    return std::string();
#endif
}

//
// DnsEnumerator
//

AdapterEnumerator::DnsEnumerator::DnsEnumerator(const AdapterEnumerator& adapter)
#if defined(OS_WIN)
    : address_(adapter.adapter_->FirstDnsServerAddress)
#endif
{
    // Nothing
}

bool AdapterEnumerator::DnsEnumerator::isAtEnd() const
{
#if defined(OS_WIN)
    return address_ == nullptr;
#else
    NOTIMPLEMENTED();
    return true;
#endif
}

void AdapterEnumerator::DnsEnumerator::advance()
{
#if defined(OS_WIN)
    address_ = address_->Next;
#else
    NOTIMPLEMENTED();
#endif
}

std::string AdapterEnumerator::DnsEnumerator::address() const
{
#if defined(OS_WIN)
    return addressToString(address_->Address);
#else
    NOTIMPLEMENTED();
    return std::string();
#endif
}

} // namespace base
