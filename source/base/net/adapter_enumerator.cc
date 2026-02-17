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

#include "base/net/adapter_enumerator.h"

#include "base/logging.h"

#if defined(Q_OS_WINDOWS)
#include <WS2tcpip.h>
#include <iphlpapi.h>
#endif // defined(Q_OS_WINDOWS)

namespace base {

namespace {

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
QString addressToString(const SOCKET_ADDRESS& address)
{
    if (!address.lpSockaddr || address.iSockaddrLength <= 0)
        return QString();

    char buffer[64] = { 0 };

    switch (address.lpSockaddr->sa_family)
    {
        case AF_INET:
        {
            sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(address.lpSockaddr);

            if (!inet_ntop(AF_INET, &addr->sin_addr, buffer, std::size(buffer)))
                return QString();
        }
        break;

        case AF_INET6:
        {
            sockaddr_in6* addr = reinterpret_cast<sockaddr_in6*>(address.lpSockaddr);

            if (!inet_ntop(AF_INET6, &addr->sin6_addr, buffer, std::size(buffer)))
                return QString();
        }
        break;

        default:
            return QString();
    }

    return buffer;
}
#endif // defined(Q_OS_WINDOWS)

} // namespace

//
// AdapterEnumerator
//

//--------------------------------------------------------------------------------------------------
AdapterEnumerator::AdapterEnumerator()
{
#if defined(Q_OS_WINDOWS)
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
#if defined(Q_OS_WINDOWS)
    return adapter_ == nullptr;
#else
    NOTIMPLEMENTED();
    return true;
#endif
}

//--------------------------------------------------------------------------------------------------
void AdapterEnumerator::advance()
{
#if defined(Q_OS_WINDOWS)
    adapter_ = adapter_->Next;
#else
    NOTIMPLEMENTED();
#endif
}

//--------------------------------------------------------------------------------------------------
QString AdapterEnumerator::adapterName() const
{
#if defined(Q_OS_WINDOWS)
    if (!adapter_->Description)
        return QString();

    return QString::fromWCharArray(adapter_->Description);
#else
    NOTIMPLEMENTED();
    return QString();
#endif
}

//--------------------------------------------------------------------------------------------------
QString AdapterEnumerator::connectionName() const
{
#if defined(Q_OS_WINDOWS)
    if (!adapter_->FriendlyName)
        return QString();

    return QString::fromWCharArray(adapter_->FriendlyName);
#else
    NOTIMPLEMENTED();
    return QString();
#endif
}

//--------------------------------------------------------------------------------------------------
QString AdapterEnumerator::interfaceType() const
{
#if defined(Q_OS_WINDOWS)
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
            return QString();
    }
#else
    NOTIMPLEMENTED();
    return QString();
#endif
}

//--------------------------------------------------------------------------------------------------
quint32 AdapterEnumerator::mtu() const
{
#if defined(Q_OS_WINDOWS)
    return adapter_->Mtu;
#else
    NOTIMPLEMENTED();
    return 0;
#endif
}

//--------------------------------------------------------------------------------------------------
quint64 AdapterEnumerator::speed() const
{
#if defined(Q_OS_WINDOWS)
    if (adapter_->TransmitLinkSpeed == std::numeric_limits<ULONG64>::max())
        return 0;

    return adapter_->TransmitLinkSpeed;
#else
    NOTIMPLEMENTED();
    return 0;
#endif
}

//--------------------------------------------------------------------------------------------------
QString AdapterEnumerator::macAddress() const
{
#if defined(Q_OS_WINDOWS)
    if (!adapter_->PhysicalAddressLength)
        return QString();

    return QString("%1-%2-%3-%4-%5-%6")
        .arg(adapter_->PhysicalAddress[0], 2, 16, QLatin1Char('0'))
        .arg(adapter_->PhysicalAddress[1], 2, 16, QLatin1Char('0'))
        .arg(adapter_->PhysicalAddress[2], 2, 16, QLatin1Char('0'))
        .arg(adapter_->PhysicalAddress[3], 2, 16, QLatin1Char('0'))
        .arg(adapter_->PhysicalAddress[4], 2, 16, QLatin1Char('0'))
        .arg(adapter_->PhysicalAddress[5], 2, 16, QLatin1Char('0'));
#else
    NOTIMPLEMENTED();
    return QString();
#endif
}

//--------------------------------------------------------------------------------------------------
bool AdapterEnumerator::isDhcp4Enabled() const
{
#if defined(Q_OS_WINDOWS)
    return !!adapter_->Dhcpv4Enabled;
#else
    NOTIMPLEMENTED();
    return false;
#endif
}

//--------------------------------------------------------------------------------------------------
QString AdapterEnumerator::dhcp4Server() const
{
#if defined(Q_OS_WINDOWS)
    return addressToString(adapter_->Dhcpv4Server);
#else
    NOTIMPLEMENTED();
    return QString();
#endif
}

//
// IpAddressEnumerator
//

AdapterEnumerator::IpAddressEnumerator::IpAddressEnumerator(const AdapterEnumerator& adapter)
#if defined(Q_OS_WINDOWS)
    : address_(adapter.adapter_->FirstUnicastAddress)
#endif // defined(Q_OS_WINDOWS)
{
    // Nothing
}

bool AdapterEnumerator::IpAddressEnumerator::isAtEnd() const
{
#if defined(Q_OS_WINDOWS)
    return address_ == nullptr;
#else
    NOTIMPLEMENTED();
    return true;
#endif
}

void AdapterEnumerator::IpAddressEnumerator::advance()
{
#if defined(Q_OS_WINDOWS)
    address_ = address_->Next;
#else
    NOTIMPLEMENTED();
#endif
}

QString AdapterEnumerator::IpAddressEnumerator::address() const
{
#if defined(Q_OS_WINDOWS)
    return addressToString(address_->Address);
#else
    NOTIMPLEMENTED();
    return QString();
#endif
}

QString AdapterEnumerator::IpAddressEnumerator::mask() const
{
#if defined(Q_OS_WINDOWS)
    in_addr addr;

    if (ConvertLengthToIpv4Mask(address_->OnLinkPrefixLength, &addr.s_addr) != NO_ERROR)
        return QString();

    char buffer[64] = { 0 };

    if (!inet_ntop(AF_INET, &addr, buffer, std::size(buffer)))
        return QString();

    return buffer;
#else
    NOTIMPLEMENTED();
    return QString();
#endif
}

//
// GatewayEnumerator
//

AdapterEnumerator::GatewayEnumerator::GatewayEnumerator(const AdapterEnumerator& adapter)
#if defined(Q_OS_WINDOWS)
    : address_(adapter.adapter_->FirstGatewayAddress)
#endif
{
    // Nothing
}

bool AdapterEnumerator::GatewayEnumerator::isAtEnd() const
{
#if defined(Q_OS_WINDOWS)
    return address_ == nullptr;
#else
    NOTIMPLEMENTED();
    return true;
#endif
}

void AdapterEnumerator::GatewayEnumerator::advance()
{
#if defined(Q_OS_WINDOWS)
    address_ = address_->Next;
#else
    NOTIMPLEMENTED();
#endif
}

QString AdapterEnumerator::GatewayEnumerator::address() const
{
#if defined(Q_OS_WINDOWS)
    return addressToString(address_->Address);
#else
    NOTIMPLEMENTED();
    return QString();
#endif
}

//
// DnsEnumerator
//

AdapterEnumerator::DnsEnumerator::DnsEnumerator(const AdapterEnumerator& adapter)
#if defined(Q_OS_WINDOWS)
    : address_(adapter.adapter_->FirstDnsServerAddress)
#endif
{
    // Nothing
}

bool AdapterEnumerator::DnsEnumerator::isAtEnd() const
{
#if defined(Q_OS_WINDOWS)
    return address_ == nullptr;
#else
    NOTIMPLEMENTED();
    return true;
#endif
}

void AdapterEnumerator::DnsEnumerator::advance()
{
#if defined(Q_OS_WINDOWS)
    address_ = address_->Next;
#else
    NOTIMPLEMENTED();
#endif
}

QString AdapterEnumerator::DnsEnumerator::address() const
{
#if defined(Q_OS_WINDOWS)
    return addressToString(address_->Address);
#else
    NOTIMPLEMENTED();
    return QString();
#endif
}

} // namespace base
